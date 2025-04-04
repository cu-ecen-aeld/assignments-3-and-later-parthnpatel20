#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <sys/queue.h>
#include "../aesd-char-driver/aesd_ioctl.h"

#define PORT 9000
#define BUFFER_SIZE 1024

// Enable AESD Char Device Usage
#define USE_AESD_CHAR_DEVICE 1

#ifdef USE_AESD_CHAR_DEVICE
    #define FILE_PATH "/dev/aesdchar"
#else
    #define FILE_PATH "/var/tmp/aesdsocketdata"
#endif

int server_socket = -1;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

// Define structure for storing thread info
typedef struct client_thread {
    pthread_t thread_id;
    int client_socket;
    LIST_ENTRY(client_thread) entries;
} client_thread_t;

// Define head for linked list
LIST_HEAD(thread_list, client_thread) thread_head;

// Signal handler for cleanup
void handle_signal(int sig) {
    syslog(LOG_INFO, "Caught signal, exiting");

    // Close all active connections
    client_thread_t *thread_data;
    while (!LIST_EMPTY(&thread_head)) {
        thread_data = LIST_FIRST(&thread_head);
        pthread_cancel(thread_data->thread_id);
        pthread_join(thread_data->thread_id, NULL);
        LIST_REMOVE(thread_data, entries);
        free(thread_data);
    }

    // Close server socket and clean up
    if (server_socket != -1) close(server_socket);

#ifndef USE_AESD_CHAR_DEVICE
    remove(FILE_PATH);
#endif

    pthread_mutex_destroy(&file_mutex);
    closelog();

    exit(EXIT_SUCCESS);
}

void *client_handler(void *arg) {
    client_thread_t *ct = (client_thread_t *)arg;
    int client_socket = ct->client_socket;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    syslog(LOG_INFO, "Client connected: socket=%d", client_socket);

    // Open file descriptor for normal operations.
    int fd = open(FILE_PATH, O_RDWR | O_APPEND | O_CREAT, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open %s: %s", FILE_PATH, strerror(errno));
        close(client_socket);
        free(ct);
        return NULL;
    }

    // accum_buf collects the  received message.
    char *accum_buf = NULL;
    size_t accum_size = 0;

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
    
        // Append received data to accum_buf.
        char *new_buf = realloc(accum_buf, accum_size + bytes_received + 1);
        
        if (!new_buf) {
            syslog(LOG_ERR, "Memory allocation failed");
            break;
        }
        
        accum_buf = new_buf;
        memcpy(accum_buf + accum_size, buffer, bytes_received);
        accum_size += bytes_received;
        accum_buf[accum_size] = '\0';

	syslog(LOG_INFO, "test log");
        // Check if the accumulated buffer starts with an IOCTL command.
        if (strncmp(accum_buf, "AESDCHAR_IOCSEEKTO:", strlen("AESDCHAR_IOCSEEKTO:")) == 0) {
        
            syslog(LOG_INFO, "ioseektofound: %s", accum_buf);
            unsigned int cmd = 0, offset = 0;
            
            // Use accum_buf directly for parsing.
            char *parse = accum_buf + strlen("AESDCHAR_IOCSEEKTO:");
            
            if (sscanf(parse, "%u,%u", &cmd, &offset) == 2) {
            
                // Open a separate FD for IOCTL processing.
                int ioctl_fd = open(FILE_PATH, O_RDWR);
                
                if (ioctl_fd < 0) {
                    syslog(LOG_ERR, "Failed to open %s for ioctl: %s", FILE_PATH, strerror(errno));
                    
                } else {
                
                    struct aesd_seekto seek = { .write_cmd = cmd, .write_cmd_offset = offset };
                    
                    if (ioctl(ioctl_fd, AESDCHAR_IOCSEEKTO, &seek) == -1) {
                        syslog(LOG_ERR, "ioctl failed: %s", strerror(errno));
                        
                    } else {
                    
                        lseek(ioctl_fd, 0, SEEK_CUR);  // match file position
                        ssize_t r;
                        
                        while ((r = read(ioctl_fd, buffer, BUFFER_SIZE)) > 0) {
                            send(client_socket, buffer, r, 0);
                        }
                        
                        // Restore FD pointer for next writes.
                        lseek(ioctl_fd, 0, SEEK_END);
                    }
                    
                    close(ioctl_fd);
                }
                
            } else {
            
                syslog(LOG_ERR, "Malformed ioctl string: %s", accum_buf);
            }
            
            // Clear the accum_buff so the ioctl string is not written.
            free(accum_buf);
            accum_buf = NULL;
            accum_size = 0;
            continue;  // Skip normal write path.
        } 

        
        // Write the entire accumulated data to the device.
        pthread_mutex_lock(&file_mutex);
        
        if (write(fd, accum_buf, accum_size) < 0) {
            syslog(LOG_ERR, "Write failed: %s", strerror(errno));
        }
        
        pthread_mutex_unlock(&file_mutex);

        // Echo back the file contents.
        pthread_mutex_lock(&file_mutex);
        lseek(fd, 0, SEEK_SET);
        ssize_t r;
        
        while ((r = read(fd, buffer, BUFFER_SIZE)) > 0) {
            send(client_socket, buffer, r, 0);
        }

        // Restore the file pointer to the end.
        lseek(fd, 0, SEEK_END);
        pthread_mutex_unlock(&file_mutex);

        // Clear the buffer for the next message.
        free(accum_buf);
        accum_buf = NULL;
        accum_size = 0;
        
    }

    free(accum_buf);
    close(fd);
    close(client_socket);
    syslog(LOG_INFO, "Client disconnected: socket=%d", client_socket);
    LIST_REMOVE(ct, entries);
    free(ct);
    return NULL;
}



// Daemonize function
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (setsid() < 0) exit(EXIT_FAILURE);
    umask(0);
    if (chdir("/") < 0) exit(EXIT_FAILURE);

    int devnull = open("/dev/null", O_RDWR);
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);

    syslog(LOG_INFO, "Daemon initialized successfully");
}

// Thread function to append timestamps every 10 seconds (only for /var/tmp/aesdsocketdata)
#ifndef USE_AESD_CHAR_DEVICE
void *timestamp_thread(void *arg) {
    while (1) {
        sleep(10);
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timestamp[BUFFER_SIZE];
        strftime(timestamp, sizeof(timestamp), "timestamp: %a, %d %b %Y %H:%M:%S %z\n", t);

        pthread_mutex_lock(&file_mutex);
        int fd = open(FILE_PATH, O_WRONLY | O_APPEND);
        if (fd != -1) {
            write(fd, timestamp, strlen(timestamp));
            close(fd);
        }
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
}
#endif

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    openlog("aesdsocket", LOG_PID, LOG_USER);

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemonize();
    }

    // Register signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Bind failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 10) == -1) {
        syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

    syslog(LOG_INFO, "Server listening on port %d", PORT);

#ifndef USE_AESD_CHAR_DEVICE
    pthread_t time_thread;
    pthread_create(&time_thread, NULL, timestamp_thread, NULL);
#endif

    LIST_INIT(&thread_head);

    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        client_thread_t *new_thread = malloc(sizeof(client_thread_t));
        if (!new_thread) {
            syslog(LOG_ERR, "Malloc failed");
            close(client_socket);
            continue;
        }

        new_thread->client_socket = client_socket;
        LIST_INSERT_HEAD(&thread_head, new_thread, entries);

        if (pthread_create(&new_thread->thread_id, NULL, client_handler, new_thread) != 0) {
            syslog(LOG_ERR, "Failed to create thread");
            free(new_thread);
            close(client_socket);
        }
    }

    close(server_socket);
    closelog();

    return 0;
}

