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

#define PORT 9000
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_socket = -1; // Global variable for server socket
int client_socket = -1; // Global variable for client socket

// Signal handler for SIGINT and SIGTERM
void handle_signal(int sig) {
    syslog(LOG_INFO, "Caught signal, exiting");
    
    // Close sockets if open
    if (client_socket != -1) close(client_socket);
    if (server_socket != -1) close(server_socket);

    // Remove file
    remove(FILE_PATH);

    // Close syslog
    closelog();

    exit(EXIT_SUCCESS);
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Register signal handlers for SIGINT and SIGTERM
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Open syslog for logging
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        return -1;
    }

    // Configure server address struct
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Bind failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

    // Listen for connections
    if (listen(server_socket, 10) == -1) {
        syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
        close(server_socket);
        return -1;
    }

    syslog(LOG_INFO, "Server listening on port %d", PORT);

    while (1) {
        // Accept a client connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        // Log accepted connection
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        // Open the file for appending
        int file_fd = open(FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (file_fd == -1) {
            syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
            close(client_socket);
            continue;
        }

        // Receive data from client
        ssize_t bytes_received;
        while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
            // Write to file
            write(file_fd, buffer, bytes_received);

            // Check if newline received, then send back full file
            if (memchr(buffer, '\n', bytes_received)) {
                close(file_fd);

                // Open the file again for reading
                file_fd = open(FILE_PATH, O_RDONLY);
                if (file_fd == -1) {
                    syslog(LOG_ERR, "Failed to open file for reading: %s", strerror(errno));
                    break;
                }

                // Read and send file contents back to client
                while ((bytes_received = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
                    send(client_socket, buffer, bytes_received, 0);
                }

                close(file_fd);
                break;
            }
        }

        // Log closed connection
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));

        // Close client socket
        close(client_socket);
        client_socket = -1;
    }

    // Close server socket (never reached)
    close(server_socket);
    closelog();

    return 0;
}

