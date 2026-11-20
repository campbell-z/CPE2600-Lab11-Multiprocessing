/*
* Filename: mandelmovie.c
* Author: Zach Campbell
* Assignment: CPE2600 Lab 12 - Multithreading
* Section: 121
* Description: Generates a Mandelbrot movie using child
* processes to create individual image frames
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>

// Parse and validate a positive integer
static int parse_positive_int(const char *argument, int *value)
{
    char *endptr;
    long result;

    errno = 0;
    result = strtol(argument, &endptr, 10);

    if (errno != 0 || endptr == argument || *endptr != '\0' ||
        result < 1 || result > INT_MAX)
    {
        return -1;
    }

    *value = (int)result;
    return 0;
}

// Wait for any one child process to finish
// Returns 0 on success and -1 if the child fails
static int wait_for_child(void)
{
    pid_t pid;
    int status;

    // Retry if waitpid is interrupted by a signal
    do
    {
        pid = waitpid(-1, &status, 0);
    }
    while (pid == -1 && errno == EINTR);
    
    if (pid == -1)
    {
        perror("waitpid");
        return -1;
    }

    // Check whether the child terminated successfully
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        fprintf(stderr, "Child process %ld failed\n", (long)pid);
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int num_processes = 1;
    int num_threads = 1;
    int opt;
    int child_failure = 0;
    // Parse command line arguments
    while ((opt = getopt(argc, argv, "p:t:")) != -1)
    {
        switch (opt)
        {
            case 'p':
                if (parse_positive_int(optarg, &num_processes) != 0)
                {
                    fprintf(stderr, "Invalid number of processes: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            
            case 't':
                if (parse_positive_int(optarg, &num_threads) != 0)
                {
                    fprintf(stderr, "Invalid number of threads: %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;


            default:
                fprintf(stderr, "Usage: %s [-p number_of_processes] [-t number of threads]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }
    // Reject unexpected positional arguments
    if (optind < argc)
    {
        fprintf(stderr, "Usage: %s [-p number_of_processes] [-t number_of_threads]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int active_children = 0;

    printf("Generateing 50 Mandelbrot frames using %d processes and %d threads per process...\n", num_processes, num_threads);
    // Generate each frame
    for (int frame = 1; frame <= 50; frame++)
    {
        pid_t pid;

        // Calculate scale for this frame
        double scale = 3.0;

        for (int i = 1; i < frame; i++)
        {
            scale *= 0.95;
        }

        // Prepare output filename and scale argument
        char filename[32];
        char scale_string[32];
        char thread_string[32];

        snprintf(filename, sizeof(filename), "mandel%d.jpg", frame);
        snprintf(scale_string, sizeof(scale_string), "%.8f", scale);
        snprintf(thread_string, sizeof(thread_string), "%d", num_threads);

        printf("Starting frame %d: scale = %s\n", frame, scale_string);
        fflush(stdout);

        // Create child process
        pid = fork();

        if (pid < 0)
        {
            perror("fork");
            return EXIT_FAILURE;
        }

        // Child
        if (pid == 0)
        {
            // Executes the mandel program
            execl("./mandel", "mandel", "-x", "-0.5", "-y", "0",
                  "-s", scale_string, "-W", "800", "-H", "800",
                  "-m", "500", "-o", filename, "-t", thread_string, (char *)NULL);

            // Reached only if execl() fails
            perror("execl");
            _exit(EXIT_FAILURE);
        }

        // Parent: record only the newly created child
        active_children ++;
        // If process limit is reached, wait for any child to finish before adding
        if (active_children >= num_processes)
        {
            if (wait_for_child() == -1)
            {
                child_failure = 1;
            }

            active_children--;
        }

        if (child_failure)
        {
            fprintf(stderr, "One or more frame-generation process(es) failed\n");
            return EXIT_FAILURE;
        }
    }
    // Wait for all remaining children to finish
    while (active_children > 0)
    {
        if (wait_for_child() == -1)
        {
            return EXIT_FAILURE;
        }

        active_children--;
    }

    printf("All 50 frames generated succesfully\n");

    return EXIT_SUCCESS;
        
}
