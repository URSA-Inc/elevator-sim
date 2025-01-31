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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ncurses.h>
#include <time.h>
#include <string.h>
#include <getopt.h>
#include <MQTTClient.h>  // Include Paho MQTT C Client
#include <limits.h>      // Include limits.h for hostname constants

#define MAX_ELEVATORS 3
#define MAX_REQUESTS 100

// Define maximum hostname length if not defined
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

// MQTT Configuration
#define ADDRESS     ""
#define USERNAME    ""
#define PASSWORD    ""
#define TOPIC       ""
#define QOS         1
#define TIMEOUT     10000L
#define CLIENTID_LEN 128  // Maximum length for the client ID
#define APP_NAME    "elevator_sim"  // Application name

typedef struct {
    int current_floor;
    int target_floor;
    int idle;
    int broken;
    int repair_intervals;
} Elevator;

typedef struct {
    int start_floor;
    int target_floor;
} Request;

// Global variables
int num_floors = 10;
int num_requests = 1000;
int interval = 2;
Elevator elevators[MAX_ELEVATORS];
Request request_queue[MAX_REQUESTS];
int queue_size = 0;
int active_requests = 0;
int simulation_running = 1;
int fire_mode = 0;
int repair_requested = 0;
int repair_time = 0;

// MQTT Client
MQTTClient client;

// Hostname
char hostname[HOST_NAME_MAX + 1];

// Function Prototypes
void draw_elevators(int idle_count, int broken_count);
void move_elevator(Elevator* elevator);
int find_nearest_idle_elevator(int request_floor);
void breakdown(int sig);
void fire_response(int sig);
void handle_repair();
int all_elevators_at_ground();
void publish_pid();
void connect_mqtt();
void get_system_hostname();

// Get system hostname
void get_system_hostname() {
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname) - 1);  // Set to "unknown" if retrieval fails
        hostname[sizeof(hostname) - 1] = '\0';
    }
}

// Create dynamic MQTT client ID
void create_client_id(char* client_id, size_t len) {
    get_system_hostname();  // Retrieve hostname
    snprintf(client_id, len, "%s-application", hostname);  // Concatenate "hostname-application"
}

// Connect to MQTT Broker
void connect_mqtt() {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    char client_id[CLIENTID_LEN];
    int rc;

    // Create the dynamic client ID
    create_client_id(client_id, sizeof(client_id));

    // Set MQTT connection options
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // Set SSL options
    ssl_opts.enableServerCertAuth = 1;  // Enable server certificate validation
    ssl_opts.sslVersion = MQTT_SSL_VERSION_TLS_1_2;
    conn_opts.ssl = &ssl_opts;

    // Create and connect the MQTT client with dynamic client ID
    MQTTClient_create(&client, ADDRESS, client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    rc = MQTTClient_connect(client, &conn_opts);

    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to MQTT broker, return code: %d\n", rc);
        MQTTClient_destroy(&client);
        exit(EXIT_FAILURE);
    }

    printf("Connected to MQTT broker at %s with client ID: %s\n", ADDRESS, client_id);
}

// Publish PID, hostname, and application name to the "status" topic
void publish_pid() {
    char payload[256];
    pid_t pid = getpid();

    // Construct the JSON payload with PID, hostname, and application name
    snprintf(payload, sizeof(payload), 
             "{\"pid\": %d, \"hostname\": \"%s\", \"application\": \"%s\"}",
             pid, hostname, APP_NAME);

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Failed to publish PID, return code: %d\n", rc);
    } else {
        printf("Published to topic '%s': %s\n", TOPIC, payload);
    }

    MQTTClient_waitForCompletion(client, token, TIMEOUT);
}

// Publish a message to the MQTT status channel
void pub_to_status(const char* message) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    // Prepare the message payload
    pubmsg.payload = (void*)message; // Casting to void* as required by MQTTClient
    pubmsg.payloadlen = (int)strlen(message);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    // Publish the message to the "status" topic
    int rc = MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Failed to publish message to topic '%s', return code: %d\n", TOPIC, rc);
    } else {
        printf("Published to topic '%s': %s\n", TOPIC, message);
    }

    // Wait for confirmation that the message was delivered
    MQTTClient_waitForCompletion(client, token, TIMEOUT);
}

// Draw the elevators' status in the ncurses window
void draw_elevators(int idle_count, int broken_count) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    if (height < num_floors + 4) {
        mvprintw(0, 0, "Terminal too small to display %d floors. Please resize your terminal.", num_floors);
        refresh();
        return;
    }

    for (int floor = num_floors - 1; floor >= 0; floor--) {
        for (int i = 0; i < MAX_ELEVATORS; i++) {
            if (elevators[i].broken) {
                mvprintw(num_floors - floor - 1, i * 10 + 5, "[XX]");
            } else if (elevators[i].current_floor == floor) {
                mvprintw(num_floors - floor - 1, i * 10 + 5, "[%d]", elevators[i].target_floor);
            } else {
                mvprintw(num_floors - floor - 1, i * 10 + 5, "[  ]");
            }
        }
    }

    mvprintw(num_floors + 1, 0, "Idle Elevators: %d | Requests in Queue: %d", idle_count, queue_size);
    mvprintw(num_floors + 2, 0, "Elevators Out of Service: %d | Repair Requested: %s | Time to Repair: %d", 
             broken_count, repair_requested ? "Yes" : "No", repair_time);

    refresh();
}

// Move the elevator towards its target floor
void move_elevator(Elevator* elevator) {
    if (elevator->broken || elevator->idle) return;

    if (elevator->current_floor < elevator->target_floor) {
        elevator->current_floor++;
    } else if (elevator->current_floor > elevator->target_floor) {
        elevator->current_floor--;
    } else {
        elevator->idle = 1;
    }
}

// Find the nearest idle elevator to the request floor
int find_nearest_idle_elevator(int request_floor) {
    int nearest = -1;
    int nearest_distance = num_floors + 1;

    for (int i = 0; i < MAX_ELEVATORS; i++) {
        if (elevators[i].idle && !elevators[i].broken) {
            int distance = abs(elevators[i].current_floor - request_floor);
            if (distance < nearest_distance) {
                nearest = i;
                nearest_distance = distance;
            }
        }
    }
    return nearest;
}

// Handle breakdown signal
void breakdown(int sig) {
    int attempts = 0;
    int elevator_id;

    do {
        elevator_id = rand() % MAX_ELEVATORS;
        attempts++;
    } while (elevators[elevator_id].broken && attempts < MAX_ELEVATORS);

    if (!elevators[elevator_id].broken) {
        elevators[elevator_id].broken = 1;
        elevators[elevator_id].repair_intervals = (rand() % 41) + 10;
        repair_requested = 1;
        repair_time = elevators[elevator_id].repair_intervals;
        mvprintw(num_floors + 4, 0, "Elevator %d broke down!\n", elevator_id + 1);
    } else {
        mvprintw(num_floors + 4, 0, "All elevators are currently broken. No new breakdown occurred.\n");
    }
    
    refresh();

}

// Handle fire response signal
void fire_response(int sig) {
    fire_mode = 1;

    // Print the fire status message at num_floors + 4
    mvprintw(num_floors + 4, 0, "Fire alarm triggered! Sending all elevators to the ground floor.");
    refresh();

    // Set all elevators to target the ground floor and mark them as active
    for (int i = 0; i < MAX_ELEVATORS; i++) {
        elevators[i].target_floor = 0;
        elevators[i].idle = 0;
        elevators[i].broken = 0;     // Force even broken elevators to descend
    }
}


// Handle elevator repairs
void handle_repair() {
    for (int i = 0; i < MAX_ELEVATORS; i++) {
        if (elevators[i].broken) {
            if (--elevators[i].repair_intervals <= 0) {
                elevators[i].broken = 0;
                repair_requested = 0;
            } else {
                repair_time = elevators[i].repair_intervals;
            }
        }
    }
}

// Check if all elevators are at the ground floor
int all_elevators_at_ground() {
    for (int i = 0; i < MAX_ELEVATORS; i++) {
        if (elevators[i].current_floor != 0 && !elevators[i].broken) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char* argv[]) {
    int opt;
    struct option long_options[] = {
        {"numreq", required_argument, NULL, 'r'},
        {"interval", required_argument, NULL, 'i'},
        {"floors", required_argument, NULL, 'f'},
        {0, 0, 0, 0}
    };

    // Parse command-line arguments
    while ((opt = getopt_long(argc, argv, "r:i:f:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                num_requests = atoi(optarg);
                break;
            case 'i':
                interval = atoi(optarg);
                break;
            case 'f':
                num_floors = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s --numreq NUM --interval SECONDS --floors FLOORS\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Initialize MQTT
    connect_mqtt();

    // Publish the PID, hostname, and application name
    publish_pid();

    signal(SIGUSR1, breakdown);
    signal(SIGUSR2, fire_response);

    // Initialize ncurses
    initscr();
    noecho();
    curs_set(FALSE);
    srand(time(NULL));

    // Initialize elevators
    for (int i = 0; i < MAX_ELEVATORS; i++) {
        elevators[i].current_floor = 0;
        elevators[i].target_floor = -1;
        elevators[i].idle = 1;
        elevators[i].broken = 0;
        elevators[i].repair_intervals = 0;
    }

    // Simulation loop
    while (simulation_running && (active_requests < num_requests || queue_size > 0 || fire_mode)) {
        if (!fire_mode && active_requests < num_requests && rand() % interval == 0) {
            Request new_request;
            new_request.start_floor = rand() % num_floors;
            new_request.target_floor = rand() % num_floors;
            request_queue[queue_size++] = new_request;
            active_requests++;
        }

        if (!fire_mode) {
            for (int i = 0; i < queue_size; i++) {
                int nearest_elevator = find_nearest_idle_elevator(request_queue[i].start_floor);
                if (nearest_elevator != -1) {
                    elevators[nearest_elevator].target_floor = request_queue[i].target_floor;
                    elevators[nearest_elevator].idle = 0;
                    for (int j = i; j < queue_size - 1; j++) {
                        request_queue[j] = request_queue[j + 1];
                    }
                    queue_size--;
                    i--;
                }
            }
        }

        int idle_count = 0;
        int broken_count = 0;
        for (int i = 0; i < MAX_ELEVATORS; i++) {
            if (!elevators[i].idle && !elevators[i].broken) {
                move_elevator(&elevators[i]);
            }
            if (elevators[i].idle || elevators[i].broken) {
                idle_count++;
            }
            if (elevators[i].broken) {
                broken_count++;
            }
        }

		if (!fire_mode) {
		
			// Check if any elevators are broken before calling handle_repair
			int any_broken = 0;
			for (int i = 0; i < MAX_ELEVATORS; i++) {
				if (elevators[i].broken) {
					any_broken = 1;
					break;
				}
			}
			
			if (any_broken) {
				handle_repair();
			}
		}
		
        draw_elevators(idle_count, broken_count);

        if (fire_mode && all_elevators_at_ground()) {
            break;
        }

        usleep(500000);
    }

    if (fire_mode) {
        sleep(5);
    }

    // End ncurses mode
    endwin();
    printf("Simulation ended due to fire response or completion of all requests.\n");

    // Disconnect from MQTT broker
    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);

    return 0;
}
