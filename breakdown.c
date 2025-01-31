/*
 * Copyright (c) 2024 David Kovar, URSA Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    char command[256];
    FILE *fp;
    pid_t pid = 0;
    char *process_name = NULL;
    int opt;

    // Seed the random number generator
    srand(time(NULL));

    // Define long options
    struct option long_options[] = {
        {"name", required_argument, 0, 'n'},
        {"pid", required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };

    // Parse command-line arguments
    while ((opt = getopt_long(argc, argv, "n:p:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'n':
                process_name = optarg;
                break;
            case 'p':
                pid = (pid_t)strtoul(optarg, NULL, 10);
                if (pid <= 0) {
                    fprintf(stderr, "Error: Invalid PID provided.\n");
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [--pid PID | --name PROCESS_NAME]\n", argv[0]);
                return 1;
        }
    }

    // If PID is not provided, look up the process name
    if (pid == 0) {
        if (process_name == NULL) {
            fprintf(stderr, "Error: Either --pid or --name must be provided.\n");
            return 1;
        }

        // Use pgrep to find the PID of the specified process
        snprintf(command, sizeof(command),
                 "pgrep -af instrumented_elevator_sim | grep -E '^./instrumented_elevator_sim|/.*instrumented_elevator_sim' | awk '{print $1}'");
        fp = popen(command, "r");
        if (fp == NULL) {
            perror("popen failed");
            return 1;
        }

        if (fgets(command, sizeof(command), fp) != NULL) {
            pid = (pid_t)strtoul(command, NULL, 10);
        } else {
            printf("%s is not running or doesn't match expected patterns.\n", process_name);
            pclose(fp);
            return 1;
        }

        pclose(fp);
    }

    // Send breakdown signal (SIGUSR1)
    if (kill(pid, SIGUSR1) == -1) {
        perror("Failed to send signal");
        return 1;
    }

    if (process_name != NULL) {
        printf("Breakdown signal sent to %s (PID %d).\n", process_name, pid);
    } else {
        printf("Breakdown signal sent to PID %d.\n", pid);
    }

    return 0;
}
