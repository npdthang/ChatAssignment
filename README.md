# TCP Multi-Client Chat Application

A simple TCP-based chat application written in C using POSIX sockets. The application follows a client-server architecture and supports multiple clients communicating with each other over a local area network (LAN).

The server uses `poll()` to handle multiple client connections without creating separate threads or processes.

## Features

- TCP client-server communication using IPv4.
- Supports up to 10 simultaneous clients.
- Multiple clients can exchange messages in real time.
- Unique nickname registration.
- Nicknames must contain only lowercase letters (`a-z`), digits (`0-9`), and underscores (`_`).
- Nickname length: 1–31 characters.
- Prevents duplicate nicknames.
- Automatically broadcasts messages to all connected clients.
- Displays notifications when users join or leave the chat.
- Handles client disconnections.
- Uses `poll()` for I/O multiplexing.
- Supports communication over a local area network (LAN).

## Project Structure

```text
tcp-chat/
├── server.c
├── client.c
└── README.md
```

## Requirements

The following tools and technologies are required:

- Linux operating system (Ubuntu recommended).
- GCC compiler.
- POSIX-compliant environment.
- TCP/IP networking support.

Install GCC and other development tools on Ubuntu:

```bash
sudo apt update
sudo apt install build-essential
```

Verify the GCC installation:

```bash
gcc --version
```

## Compilation

Open a terminal in the project directory.

### 1. Compile the Server

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic server.c -o server
```

### 2. Compile the Client

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic client.c -o client
```

After compilation, the following executable files should be available:

```text
tcp-chat/
├── server.c
├── client.c
├── server
├── client
└── README.md
```

## Running the Application

The application requires one server and one or more clients.

### 1. Find the Server's LAN IP Address

On the computer that will run the server, execute:

```bash
hostname -I
```

Example output:

```text
192.168.1.10
```

Use the appropriate IPv4 address associated with your local network interface.

### 2. Start the Server

Run the following command:

```bash
./server 192.168.1.10 5050
```

Replace `192.168.1.10` with the actual LAN IP address of your server.

The server will display:

```text
Chat server listening on 192.168.1.10:5050 (max 10 clients)
```

The server is now ready to accept incoming connections.

### 3. Start a Client

Open another terminal on the same computer or a different computer connected to the same LAN.

Execute:

```bash
./client 192.168.1.10 5050
```

The client will connect to the server and request a nickname:

```text
Enter nickname (lowercase, no spaces):
Nickname:
```

Enter a valid nickname:

```text
thang
```

If the nickname is accepted, the server responds:

```text
OK
```

The client enters the chat room:

```text
You are in the chat. Type a message and press Enter.
Ctrl+D finishes sending.
```

### 4. Connect Multiple Clients

Open additional terminals and execute the client command:

```bash
./client 192.168.1.10 5050
```

Each client must register a unique nickname.

Example:

| Client | Nickname |
|---|---|
| Client 1 | thang |
| Client 2 | alice |
| Client 3 | bob |
| Client 4 | charlie |

The server supports a maximum of 10 simultaneous clients.

## Chat Example

### Server Terminal

```text
Chat server listening on 192.168.1.10:5050 (max 10 clients)

New connection from 192.168.1.11. Waiting for nickname...
*** thang joined the chat ***

New connection from 192.168.1.12. Waiting for nickname...
*** alice joined the chat ***

[thang] Hello everyone!
[alice] Hi thang!
```

### Client 1 (thang)

```text
You are in the chat. Type a message and press Enter.
Ctrl+D finishes sending.

[alice] Hi thang!
Hello everyone!
```

### Client 2 (alice)

```text
You are in the chat. Type a message and press Enter.
Ctrl+D finishes sending.

[thang] Hello everyone!
Hi thang!
```

Messages sent by one client are broadcast to all connected clients, including the sender.

## Disconnecting from the Chat

Press:

```text
Ctrl+D
```

This closes the client's sending direction using `shutdown(fd, SHUT_WR)`. The client can continue receiving messages until the server closes the connection.

When a client disconnects, the server broadcasts a notification:

```text
*** thang left the chat ***
```

To stop the server, press:

```text
Ctrl+C
```

## Network Configuration

If the server and clients are running on different computers, make sure:

1. All computers are connected to the same LAN or can communicate through a routed network.
2. The server uses the correct LAN IPv4 address.
3. The selected TCP port is not being used by another application.
4. The server firewall allows incoming TCP connections on port `5050`.

For Ubuntu with UFW, allow the chat port:

```bash
sudo ufw allow 5050/tcp
```

Check the firewall status:

```bash
sudo ufw status
```

## Technical Overview

### Server

The server is responsible for:

- Creating and binding a TCP listening socket.
- Accepting incoming client connections.
- Managing up to 10 connected clients.
- Receiving and processing client messages.
- Validating and registering nicknames.
- Broadcasting messages to connected clients.
- Detecting disconnected clients.

### Client

The client is responsible for:

- Connecting to the server using IPv4.
- Registering a nickname.
- Sending messages to the server.
- Receiving messages from other clients.
- Monitoring keyboard input and socket events.

### I/O Multiplexing

Both server and client use the POSIX `poll()` system call.

The server monitors:
- The listening socket for new connections.
- Client sockets for incoming data.

The client monitors:
- Standard input (`STDIN_FILENO`) for keyboard input.
- The server socket for incoming messages.

This approach allows the application to handle multiple I/O events without requiring a separate thread for each client.

## Limitations

- Maximum of 10 simultaneous clients.
- Communication is unencrypted.
- Nicknames are not persistent and must be registered again after reconnecting.
- Messages are limited to 1023 characters.
- No message history or private messaging.
- Designed for Linux and POSIX-compatible systems.

## License

This project is intended for educational purposes and for learning TCP/IP socket programming in C.