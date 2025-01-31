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
#include <unistd.h>
#include <getopt.h>

int main(int argc, char *argv[]) {
    pid_t pid = 0; // Variable to store the provided PID
    int opt;

    // Parse command-line options
    struct option long_options[] = {
        {"pid", required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "p:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'p':
                pid = (pid_t)strtoul(optarg, NULL, 10);
                break;
            default:
                fprintf(stderr, "Usage: %s --pid <pid>\n", argv[0]);
                return 1;
        }
    }

    // Ensure PID was provided
    if (pid == 0) {
        fprintf(stderr, "Error: --pid option is required.\n");
        fprintf(stderr, "Usage: %s --pid <pid>\n", argv[0]);
        return 1;
    }

    // Send fire response signal (SIGUSR2)
    if (kill(pid, SIGUSR2) == -1) {
        perror("Failed to send signal");
        return 1;
    }

    printf("Fire response signal sent to PID %d.\n", pid);
    return 0;
}
