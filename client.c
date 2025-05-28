#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char server_response[BUFFER_SIZE] = {0};
    char user_input[BUFFER_SIZE] = {0};
    ssize_t bytes_received;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported for server IP: %s\n", SERVER_IP);
        close(sock);
        return -1;
    }

    printf("Connecting to server %s:%d...\n", SERVER_IP, SERVER_PORT);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        return -1;
    }
    printf("Connected to server.\n\n");

    // Game loop
    while (1) {
        memset(server_response, 0, BUFFER_SIZE);
        bytes_received = recv(sock, server_response, BUFFER_SIZE - 1, 0);

        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Server closed the connection.\n");
            } else {
                perror("recv failed");
            }
            break;
        }
        server_response[bytes_received] = '\0';
        printf("%s", server_response); // Server messages include newlines

        // Check for game state cues from the server
        if (strstr(server_response, "YOUR_TURN")) {
            // The prompt is part of the YOUR_TURN message from server
            // e.g., "YOUR_TURN: Player X, enter move (1-9) or 'chat <message>': "
            printf("> "); // Simple prompt indicator
            if (fgets(user_input, BUFFER_SIZE, stdin) == NULL) {
                printf("Error reading input or EOF, exiting.\n");
                break; // Or send a quit message
            }
            // fgets keeps the newline, server-side strcspn will remove it.
            // Or remove it here: user_input[strcspn(user_input, "\n")] = 0;
            
            if (send(sock, user_input, strlen(user_input), 0) < 0) {
                perror("send failed");
                break;
            }
        } else if (strstr(server_response, "GAME_OVER:")) {
            printf("Game has ended. Thank you for playing!\n");
            break;
        }
        // Other messages like BOARD:, CHAT_MSG:, WAIT:, INFO:, INVALID_MOVE:, INVALID_INPUT:
        // are just displayed, and the client waits for the next server instruction or game update.
    }

    printf("Closing connection.\n");
    close(sock);
    return 0;
}