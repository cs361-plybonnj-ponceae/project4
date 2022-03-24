/*
 * project4.c
 * Team: 07
 * Names: Nic Plybon and Adrien Ponce
 * Honor code statement: This code complies with the JMU Honor Code
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include "common.h"
#include "classify.h"
#ifdef GRADING // do not delete this or the next two lines
#include "gradelib.h"
#endif


#define NUM_PROCESSES 5

// This is the recommended struct for sending a message about
// a cluster's type through the pipe. You need not use this,
// but I would recommend that you do.
struct msg {
    int msg_cluster_number;  // The cluster number from the input file
    unsigned char msg_cluster_type;  // The type of the above cluster
    // as determined by the classifier
};

int main(int argc, char *argv[])
{
    int input_fd;
    int classification_fd;
    pid_t pid;
    int pipefd[2];
    struct msg message;
    int start_cluster; // When a child process is created, this variable must
    // contain the first cluster in the input file it
    // needs to classify
    int clusters_to_process; // This variable must contain the number of
    // clusters a child process needs to classify

    // The user must supply a data file to be used as input
    if (argc != 2) {
        printf("Usage: %s data_file\n", argv[0]);
        return 1;
    }

    // Open input file for reading, exiting with error if open() fails
    input_fd = open(argv[1], O_RDONLY);
    if (input_fd < 0) {
        printf("Error opening file \"%s\" for reading: %s\n", argv[1], strerror(errno));
        return 1;
    }

    //use a struct to get size of input file
    struct stat sb;
    if (fstat(input_fd, &sb) == -1) {
        printf("Error: %s", strerror(errno));
        return 1;
    }

    // Open classification file for writing. Create file if it does not
    // exist. Exit with error if open() fails.
    classification_fd = open(CLASSIFICATION_FILE, O_WRONLY | O_CREAT, 0600);
    if (classification_fd < 0) {
        printf("Error creating file \"%s\": %s\n", CLASSIFICATION_FILE, strerror(errno));
        return 1;
    }

    // Create the pipe here. Exit with error if this fails.
    if (pipe(pipefd) < 0) {
        printf("Error: %s", strerror(errno));
        return 1;
    }

    // The pipe must be created at this point
#ifdef GRADING // do not delete this or you will lose points
    test_pipefd(pipefd, 0);
#endif

    // total number of clusters in the input file
    int total_clusters = sb.st_size / CLUSTER_SIZE;
    // a counter used to keep track of cluster sizes that are not divisible by NUM_PROCESSES
    int remainder = total_clusters % NUM_PROCESSES + 1;
    // a variable used to remember the value of the remainder
    int r = remainder - 1;
    start_cluster = 0;
    clusters_to_process = total_clusters / NUM_PROCESSES;

    // Calculations for Sample Clusters
    //input file = 6207 clusters
    //children = 5
    //child1 1242 cluster 0
    //child2 1242 cluster 1242
    //child3 1241 cluster 2484
    //child4 1241 cluster 3725
    //child5 1241 cluster 4966

    // Fork NUM_PROCESS number of child processes
    for (int i = 0; i < NUM_PROCESSES; i++) {
        // decrement remainder on each iteration
        remainder--;
        pid = fork();
        // Exit if fork() fails.
        if (pid == -1) {
            exit(1);
        }
        // This is the place to write the code for the child processes
        else if (pid == 0) {

            // In this else if block, you need to implement the entire logic
            // for the child processes to be aware of which clusters
            // they need to process, classify them, and create a message
            // for each cluster to be written to the pipe.

            // with no remainder remaining
            if (remainder <= 0) {
                // compute the starting cluster
                start_cluster = clusters_to_process * i + r;
            } else {
                // tack a one onto every cluster that has the remainder still remaining
                clusters_to_process += 1;
                start_cluster = clusters_to_process * i;
            }

            char cluster_data[CLUSTER_SIZE];
            int bytes_read;
            message.msg_cluster_number = start_cluster;

            // At this point, the child must know its start cluster and
            // the number of clusters to process.

#ifdef GRADING // do not delete this or you will lose points
            printf("Child process %d\n\tStart cluster: %d\n\tClusters to process: %d\n",
                   getpid(), start_cluster, clusters_to_process);
#endif

            // At this point the pipe must be fully set up for the child
            // This code must be executed before you start iterating over the input
            // file and before you generate and write messages.
            close(pipefd[0]);

#ifdef GRADING // do not delete this or you will lose points
            test_pipefd(pipefd, getpid());
#endif


            // Implement the main loop for the child process below this line
            while ((bytes_read = read(input_fd, &cluster_data, CLUSTER_SIZE)) > 0) {
                assert(bytes_read == CLUSTER_SIZE);
                message.msg_cluster_type = TYPE_UNCLASSIFIED;

                // Checks that the current cluster is of type JPG by looking for
                // its body, and if it has either a header or a footer on each
                // iteration. This helps avoid false negatives
                if (has_jpg_body(cluster_data)) {
                    message.msg_cluster_type = TYPE_IS_JPG;
                    if (has_jpg_header(cluster_data)
                            | has_jpg_footer(cluster_data)) {
                        message.msg_cluster_type = TYPE_IS_JPG;
                    }
                }

                // Checks that the current cluster is of type HTML by looking for
                // its body, and if it has either a header or a footer on each
                // iteration. This helps avoid false negatives
                if (has_html_body(cluster_data)) {
                    message.msg_cluster_type = TYPE_IS_HTML;
                    if (has_html_header(cluster_data)
                            | has_html_footer(cluster_data)) {
                        message.msg_cluster_type = TYPE_IS_HTML;
                    }
                }

                // Check if the current cluster is a JPG header
                // and set the file attribute to 0x03
                if (has_jpg_header(cluster_data)) {
                    message.msg_cluster_type = TYPE_IS_JPG | TYPE_JPG_HEADER;
                }

                // Check if the current cluster is a HTML header
                // and set the file attribute to 0x18
                if (has_html_header(cluster_data)) {
                    message.msg_cluster_type = TYPE_IS_HTML | TYPE_HTML_HEADER;
                }

                // Check if the current cluster is a JPG footer
                // and set the file attribute to 0x05
                if (has_jpg_footer(cluster_data)) {
                    message.msg_cluster_type = TYPE_IS_JPG | TYPE_JPG_FOOTER;
                }

                // Check if the current cluster is a HTML footer
                // and set the file attribute to 0x28
                if (has_html_footer(cluster_data)) {
                    message.msg_cluster_type = TYPE_IS_HTML | TYPE_HTML_FOOTER;
                }

                // Single HTML contains both a header and a footer followed by zero bytes.
                // Set the file attribtue to 0x38
                if (has_html_header(cluster_data)
                        && has_html_footer(cluster_data)) {
                    message.msg_cluster_type = TYPE_IS_HTML | TYPE_HTML_HEADER | TYPE_HTML_FOOTER;
                }

                // Single JPG contains both a header and a footer followed by zero bytes.
                // Set the file attribute to 0x07
                if (has_jpg_header(cluster_data)
                        && has_jpg_footer(cluster_data)) {
                    message.msg_cluster_type = TYPE_IS_JPG | TYPE_JPG_HEADER | TYPE_JPG_FOOTER;
                }

                // Check if the current cluster is invalid
                // and set the file attribute to 0x80
                if (message.msg_cluster_type == TYPE_UNCLASSIFIED) {
                    message.msg_cluster_type = TYPE_UNKNOWN;
                }

                write(pipefd[1], &message, sizeof(message));
                message.msg_cluster_number++;
            }

            exit(0); // This line needs to be the last one for the child
            // process code. Do not delete this!
        }
    }

    // All the code for the parent's handling of the messages and
    // creating the classification file needs to go in the block below
    close(pipefd[1]);

    // At this point, the pipe must be fully set up for the parent
#ifdef GRADING // do not delete this or you will lose points
    test_pipefd(pipefd, 0);
#endif

    // Read one message from the pipe at a time
    while (read(pipefd[0], &message, sizeof(message)) > 0) {
        // In this loop, you need to implement the processing of
        // each message sent by a child process. Based on the content,
        // a proper entry in the classification file needs to be written.
        write(classification_fd, &message.msg_cluster_type, 1);
    }


    // Resource cleanup
    close(classification_fd);
    close(input_fd);
    return 0;
};
