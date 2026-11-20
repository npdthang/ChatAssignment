#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define NICK_LEN 32
#define LINE_LEN 1024

typedef struct {
    int fd;
    int active;
    int named;
    char nickname[NICK_LEN];
    char input[LINE_LEN];
    size_t input_len;
} Client;

static int send_all(int fd, const char *data, size_t length)
{
    while (length > 0) {
        ssize_t n = send(fd, data, length, MSG_NOSIGNAL);
        if (n == -1 && errno == EINTR)
            continue;
        if (n <= 0) {
            if (n == -1) perror("send");
            return -1;
        }
        data += n;
        length -= (size_t)n;
    }
    return 0;
}

static int valid_nickname(const char *name)
{
    size_t i;
    size_t len = strlen(name);

    if (len == 0 || len >= NICK_LEN)
        return 0;

    for (i = 0; i < len; i++) {
        if (!((name[i] >= 'a' && name[i] <= 'z') ||
              (name[i] >= '0' && name[i] <= '9') ||
              name[i] == '_'))
            return 0;
    }
    return 1;
}

static int nickname_taken(Client clients[], const char *name)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].named &&
            strcmp(clients[i].nickname, name) == 0)
            return 1;
    }
    return 0;
}

static void broadcast(Client clients[], const char *message, int except_fd)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].named &&
            clients[i].fd != except_fd) {
            if (send_all(clients[i].fd, message, strlen(message)) == -1) {
                close(clients[i].fd);
                clients[i].fd = -1;
                clients[i].active = 0;
                clients[i].named = 0;
            }
        }
    }
}

static void disconnect_client(Client clients[], int index)
{
    char message[LINE_LEN + NICK_LEN + 32];
    int fd = clients[index].fd;

    if (clients[index].active && clients[index].named) {
        snprintf(message, sizeof(message), "*** %s left the chat ***\n",
                 clients[index].nickname);
        printf("%s", message);
        broadcast(clients, message, fd);
    }

    if (fd >= 0)
        close(fd);

    memset(&clients[index], 0, sizeof(clients[index]));
    clients[index].fd = -1;
}

static int process_line(Client clients[], int index, char *line)
{
    Client *client = &clients[index];
    char message[LINE_LEN + NICK_LEN + 8];

    /* The first line sent by a client must be its nickname. */
    if (!client->named) {
        if (!valid_nickname(line)) {
            const char *error =
                "ERROR: nickname must be 1-31 lowercase letters, digits or '_'.\n";
            (void)send_all(client->fd, error, strlen(error));
            return -1;
        }
        if (nickname_taken(clients, line)) {
            const char *error = "ERROR: nickname already in use.\n";
            (void)send_all(client->fd, error, strlen(error));
            return -1;
        }

        strcpy(client->nickname, line);
        client->named = 1;

        if (send_all(client->fd, "OK\n", 3) == -1)
            return -1;

        snprintf(message, sizeof(message), "*** %s joined the chat ***\n",
                 client->nickname);
        printf("%s", message);
        broadcast(clients, message, client->fd);
        return 0;
    }

    if (line[0] == '\0')
        return 0;

    snprintf(message, sizeof(message), "[%s] %s\n", client->nickname, line);
    printf("%s", message);
    broadcast(clients, message, -1);
    return 0;
}

int main(int argc, char **argv)
{
    struct sockaddr_in address = {0};
    Client clients[MAX_CLIENTS];
    struct pollfd pollfds[MAX_CLIENTS + 1];
    int listener;
    int i;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s SERVER_LAN_IP PORT\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *end = NULL;
    errno = 0;
    long port = strtol(argv[2], &end, 10);
    if (errno || end == argv[2] || *end != '\0' || port < 1 || port > 65535) {
        fputs("Invalid port (use 1-65535).\n", stderr);
        return EXIT_FAILURE;
    }

    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, argv[1], &address.sin_addr) != 1) {
        fputs("Invalid numeric IPv4 address.\n", stderr);
        return EXIT_FAILURE;
    }

    for (i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;

    /* Avoid terminating the whole server if a client disconnects mid-send. */
    signal(SIGPIPE, SIG_IGN);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int reuse = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        perror("setsockopt");
        close(listener);
        return EXIT_FAILURE;
    }

    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(listener);
        return EXIT_FAILURE;
    }

    if (listen(listener, MAX_CLIENTS) == -1) {
        perror("listen");
        close(listener);
        return EXIT_FAILURE;
    }

    printf("Chat server listening on %s:%ld (max %d clients)\n",
           argv[1], port, MAX_CLIENTS);

    for (;;) {
        int slot_for_poll[MAX_CLIENTS + 1];

        pollfds[0].fd = listener;
        pollfds[0].events = POLLIN;
        pollfds[0].revents = 0;
        slot_for_poll[0] = -1;

        for (i = 0; i < MAX_CLIENTS; i++) {
            pollfds[i + 1].fd = clients[i].active ? clients[i].fd : -1;
            pollfds[i + 1].events = POLLIN;
            pollfds[i + 1].revents = 0;
            slot_for_poll[i + 1] = i;
        }

        int ready = poll(pollfds, MAX_CLIENTS + 1, -1);
        if (ready == -1) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        if (pollfds[0].revents & POLLIN) {
            struct sockaddr_in peer_addr;
            socklen_t peer_len = sizeof(peer_addr);
            int peer = accept(listener, (struct sockaddr *)&peer_addr, &peer_len);

            if (peer == -1) {
                if (errno != EINTR) perror("accept");
            } else {
                int free_slot = -1;
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (!clients[i].active) {
                        free_slot = i;
                        break;
                    }
                }

                if (free_slot == -1) {
                    const char *error = "ERROR: chat room is full.\n";
                    (void)send_all(peer, error, strlen(error));
                    close(peer);
                    puts("Rejected client: room is full.");
                } else {
                    clients[free_slot].fd = peer;
                    clients[free_slot].active = 1;
                    clients[free_slot].named = 0;
                    clients[free_slot].input_len = 0;
                    printf("New connection from %s. Waiting for nickname...\n",
                           inet_ntoa(peer_addr.sin_addr));
                    const char *prompt = "Enter nickname (lowercase, no spaces):\n";
                    (void)send_all(peer, prompt, strlen(prompt));
                }
            }
        }

        for (i = 1; i <= MAX_CLIENTS; i++) {
            int index = slot_for_poll[i];
            short revents = pollfds[i].revents;

            if (index < 0 || !clients[index].active || revents == 0)
                continue;

            if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                disconnect_client(clients, index);
                continue;
            }

            if (revents & POLLIN) {
                char chunk[512];
                ssize_t n = recv(clients[index].fd, chunk, sizeof(chunk), 0);

                if (n == 0) {
                    disconnect_client(clients, index);
                    continue;
                }
                if (n < 0) {
                    if (errno == EINTR) continue;
                    perror("recv");
                    disconnect_client(clients, index);
                    continue;
                }

                int failed = 0;
                ssize_t j;
                for (j = 0; j < n; j++) {
                    char ch = chunk[j];

                    if (ch == '\n') {
                        clients[index].input[clients[index].input_len] = '\0';
                        if (clients[index].input_len > 0 &&
                            clients[index].input[clients[index].input_len - 1] == '\r')
                            clients[index].input[clients[index].input_len - 1] = '\0';

                        if (process_line(clients, index, clients[index].input) == -1) {
                            failed = 1;
                            break;
                        }
                        clients[index].input_len = 0;
                    } else {
                        if (clients[index].input_len >= LINE_LEN - 1) {
                            const char *error = "ERROR: message too long.\n";
                            (void)send_all(clients[index].fd, error, strlen(error));
                            failed = 1;
                            break;
                        }
                        clients[index].input[clients[index].input_len++] = ch;
                    }
                }

                if (failed && clients[index].active)
                    disconnect_client(clients, index);
            }
        }
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active)
            close(clients[i].fd);
    }
    close(listener);
    return EXIT_FAILURE;
}
