#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BACKLOG 10

void signal_handler(int signum) {
    if (signum == SIGTERM || signum == SIGINT) {
        syslog(LOG_INFO, "Daemon terminated by signal %d.", signum);
        closelog();
        exit(EXIT_SUCCESS);
    }
}

void execute_command(int client_sock, char *command) {
    char *shell_path = "/Users/j2377312/CLionProjects/yash/yash/yash";  // PATH TO SHELL UPDATE TO YOUR SHELL LOCATION
    char *args[] = {shell_path, command, NULL};

    pid_t pid = fork();
    if (pid == 0) {
        dup2(client_sock, STDOUT_FILENO);
        dup2(client_sock, STDERR_FILENO);

        execv(shell_path, args);

        perror("execv");
        exit(EXIT_FAILURE);
    }
    if (pid < 0) {
        syslog(LOG_ERR, "Failed to fork for executing custom shell");
    }

    wait(NULL);
}

void handle_client(int client_sock) {
    char buffer[1024];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        const int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0) {
            break;
        }

        buffer[bytes_received] = '\0';

        syslog(LOG_INFO, "Received command: %s", buffer);

        execute_command(client_sock, buffer);
    }

    close(client_sock);
}

void start_daemon() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    pid_t pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "Fork failed. Exiting.");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    openlog("shell_daemon", LOG_NOWAIT | LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Daemon started.");

    pid_t sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "Failed to create new session. Exiting.");
        exit(EXIT_FAILURE);
    }

    if ((chdir("/")) < 0) {
        syslog(LOG_ERR, "Failed to change directory to root. Exiting.");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_RDWR);

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        syslog(LOG_ERR, "Failed to create socket. Exiting.");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Socket binding failed. Exiting.");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, BACKLOG) < 0) {
        syslog(LOG_ERR, "Socket listen failed. Exiting.");
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Daemon listening on port %d", PORT);

    while (1) {
        int client_sock = accept(server_sock, (struct sockaddr *) &client_addr, &addr_len);
        if (client_sock < 0) {
            syslog(LOG_ERR, "Client connection failed.");
            continue;
        }

        pid = fork();
        if (pid < 0) {
            syslog(LOG_ERR, "Fork failed. Closing client socket.");
            close(client_sock);
            continue;
        }
        if (pid == 0) {
            close(server_sock);
            handle_client(client_sock);
            exit(EXIT_SUCCESS);
        }
        close(client_sock);
    }
}

int main() {
    start_daemon();
    return 0;
}
