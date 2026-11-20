#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define NICK_LEN 32
#define BUFFER_SIZE 1024

static int send_all(int fd, const char *data, size_t length)
{
    while (length > 0) {
        ssize_t n = send(fd, data, length, 0);
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

/* Read one newline-terminated server response (used for nickname handshake). */
static int receive_line(int fd, char *buffer, size_t capacity)
{
    size_t used = 0;

    while (used + 1 < capacity) {
        char ch;
        ssize_t n = recv(fd, &ch, 1, 0);

        if (n == -1 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1;
        if (ch == '\n')
            break;
        buffer[used++] = ch;
    }

    buffer[used] = '\0';
    return 0;
}

static int chat(int fd)
{
    struct pollfd events[2] = {
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = fd, .events = POLLIN }
    };
    int sending = 1;
    int receiving = 1;

    puts("You are in the chat. Type a message and press Enter.");
    puts("Ctrl+D finishes sending.");

    while (sending || receiving) {
        if (poll(events, 2, -1) == -1) {
            if (errno == EINTR) continue;
            perror("poll");
            return EXIT_FAILURE;
        }

        if (events[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            char buffer[BUFFER_SIZE];
            ssize_t n = recv(fd, buffer, sizeof(buffer), 0);

            if (n > 0) {
                if (fwrite(buffer, 1, (size_t)n, stdout) != (size_t)n ||
                    fflush(stdout) == EOF) {
                    perror("stdout");
                    return EXIT_FAILURE;
                }
            } else if (n == 0) {
                puts("\nServer closed the connection.");
                receiving = 0;
                events[1].fd = -1;
            } else if (errno != EINTR) {
                perror("recv");
                return EXIT_FAILURE;
            }
        }

        if (events[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            char buffer[BUFFER_SIZE];
            ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));

            if (n > 0) {
                if (send_all(fd, buffer, (size_t)n) == -1)
                    return EXIT_FAILURE;
            } else if (n == 0) {
                if (shutdown(fd, SHUT_WR) == -1) {
                    perror("shutdown");
                    return EXIT_FAILURE;
                }
                sending = 0;
                events[0].fd = -1;
                puts("\nFinished sending. Waiting for chat messages...");
            } else if (errno != EINTR) {
                perror("read stdin");
                return EXIT_FAILURE;
            }
        }

        if ((events[0].revents | events[1].revents) & POLLNVAL) {
            fputs("Invalid file descriptor.\n", stderr);
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    struct sockaddr_in address = {0};
    char nickname[NICK_LEN];
    char response[128];

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

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("connect");
        close(fd);
        return EXIT_FAILURE;
    }

    if (receive_line(fd, response, sizeof(response)) == -1) {
        fputs("Server disconnected during connection setup.\n", stderr);
        close(fd);
        return EXIT_FAILURE;
    }
    printf("%s", response);

    printf("Nickname: ");
    fflush(stdout);

    if (fgets(nickname, sizeof(nickname), stdin) == NULL) {
        close(fd);
        return EXIT_FAILURE;
    }
    nickname[strcspn(nickname, "\r\n")] = '\0';

    if (!valid_nickname(nickname)) {
        fputs("Nickname must be 1-31 lowercase letters, digits or '_'.\n", stderr);
        close(fd);
        return EXIT_FAILURE;
    }

    char nick_line[NICK_LEN + 2];
    snprintf(nick_line, sizeof(nick_line), "%s\n", nickname);

    if (send_all(fd, nick_line, strlen(nick_line)) == -1) {
        close(fd);
        return EXIT_FAILURE;
    }

    if (receive_line(fd, response, sizeof(response)) == -1) {
        fputs("Server disconnected during nickname registration.\n", stderr);
        close(fd);
        return EXIT_FAILURE;
    }

    if (strcmp(response, "OK") != 0) {
        fprintf(stderr, "%s\n", response);
        close(fd);
        return EXIT_FAILURE;
    }

    int result = chat(fd);
    close(fd);
    return result;
}
