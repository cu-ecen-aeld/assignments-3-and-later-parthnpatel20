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

#define PORT 9000
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_socket = -1;
int client_socket = -1;

void handle_signal(int sig) {
    syslog(LOG_INFO, "Caught signal, exiting");
    if (client_socket != -1) close(client_socket);
    if (server_socket != -1) close(server_socket);
    remove(FILE_PATH);
    closelog();
    exit(EXIT_SUCCESS);
}

void daemonize() {
    pid_t pid, sid;
    
    pid = fork();
    if (pid < 0) { exit(EXIT_FAILURE); }
    if (pid > 0) { exit(EXIT_SUCCESS); }

    sid = setsid();
    if (sid < 0) { exit(EXIT_FAILURE); }

    umask(0);
    if (chdir("/") < 0) { exit(EXIT_FAILURE); }

    int devnull = open("/dev/null", O_RDWR);
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);  // Proper cleanup

    syslog(LOG_INFO, "Daemon initialized successfully");
}

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    openlog("aesdsocket", LOG_PID, LOG_USER);

    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemonize();
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

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

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        FILE *file = fopen(FILE_PATH, "a+");
        if (!file) {
            syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
            close(client_socket);
            continue;
        }

        ssize_t bytes_received;
        while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, bytes_received, file);
            fflush(file);

            if (memchr(buffer, '\n', bytes_received)) {
                rewind(file);
                while ((bytes_received = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                    send(client_socket, buffer, bytes_received, 0);
                }
                break;
            }
        }

        fclose(file);
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));

        close(client_socket);
        client_socket = -1;
    }

    close(server_socket);
    closelog();

    return 0;
}

