#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h> // For threading

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PENDING_CONNECTIONS 10

char player_symbols[2] = {'X', 'O'};

// Global data for matchmaking (protected by mutex) 
pthread_mutex_t matchmaking_mutex = PTHREAD_MUTEX_INITIALIZER;
int waiting_player_socket = -1;
char waiting_player_ip[INET_ADDRSTRLEN]; // Lưu địa chỉ của người đợi
int waiting_player_port; 

// Global statistics (protected by mutex) 
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
int total_games_played = 0;
int x_wins = 0;
int o_wins = 0;
int draws = 0;

// Struct to pass arguments to the game thread 
typedef struct {
    int player_sockets[2];
    char player_ips[2][INET_ADDRSTRLEN];
    int player_ports[2];
    char local_board[3][3];
} game_session_args_t;


// Game Logic Helper Functions (operate on game_session_args_t's local_board) 
void initialize_board_local(game_session_args_t* game_args) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            game_args->local_board[i][j] = ' ';
        }
    }
}

void format_board_string_local(game_session_args_t* game_args, char* buffer) {
    // Note: The buffer passed here is 'board_str' which is BUFFER_SIZE.
    // This function ensures its own output fits within that.
    snprintf(buffer, BUFFER_SIZE, "BOARD:\n %c | %c | %c \n---|---|---\n %c | %c | %c \n---|---|---\n %c | %c | %c \n",
             game_args->local_board[0][0], game_args->local_board[0][1], game_args->local_board[0][2],
             game_args->local_board[1][0], game_args->local_board[1][1], game_args->local_board[1][2],
             game_args->local_board[2][0], game_args->local_board[2][1], game_args->local_board[2][2]);
}

int check_win_local(game_session_args_t* game_args, char player_symbol) {
    for (int i = 0; i < 3; i++) {
        if ((game_args->local_board[i][0] == player_symbol && game_args->local_board[i][1] == player_symbol && game_args->local_board[i][2] == player_symbol) ||
            (game_args->local_board[0][i] == player_symbol && game_args->local_board[1][i] == player_symbol && game_args->local_board[2][i] == player_symbol)) {
            return 1;
        }
    }
    if ((game_args->local_board[0][0] == player_symbol && game_args->local_board[1][1] == player_symbol && game_args->local_board[2][2] == player_symbol) ||
        (game_args->local_board[0][2] == player_symbol && game_args->local_board[1][1] == player_symbol && game_args->local_board[2][0] == player_symbol)) {
        return 1;
    }
    return 0;
}

int check_draw_local(game_session_args_t* game_args) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (game_args->local_board[i][j] == ' ') {
                return 0;
            }
        }
    }
    return 1;
}

int make_move_local(game_session_args_t* game_args, int move, char player_symbol) {
    if (move < 1 || move > 9) return 0;
    int row = (move - 1) / 3;
    int col = (move - 1) % 3;
    if (game_args->local_board[row][col] == ' ') {
        game_args->local_board[row][col] = player_symbol;
        return 1;
    }
    return 0; // Cell already taken
}

// Main Game Playing Function (runs in a separate thread per game)
void play_tic_tac_toe(game_session_args_t* game_args) {
    char buffer[BUFFER_SIZE]; // For receiving client input
    char board_str[BUFFER_SIZE]; // For the formatted board string
    // MODIFICATION HERE: Increased size for message_to_send
    char message_to_send[BUFFER_SIZE + 256]; // For composing messages to send, + 256 help chat in vietnamese

    int current_player_idx = 0; // Player 0 ('X') starts
    int game_over = 0;
    int turn_count = 0;
    ssize_t bytes_received;

    initialize_board_local(game_args);
    printf("INFO: Game started between %s:%d (X) and %s:%d (O)\n",
           game_args->player_ips[0], game_args->player_ports[0],
           game_args->player_ips[1], game_args->player_ports[1]);

    // Inform players of their symbols and game start
    // Using sizeof(message_to_send) for all snprintf to message_to_send
    snprintf(message_to_send, sizeof(message_to_send), "INFO: Game Start! You are Player %c.\n", player_symbols[0]);
    send(game_args->player_sockets[0], message_to_send, strlen(message_to_send), 0);
    snprintf(message_to_send, sizeof(message_to_send), "INFO: Game Start! You are Player %c.\n", player_symbols[1]);
    send(game_args->player_sockets[1], message_to_send, strlen(message_to_send), 0);

    while (!game_over) {
        int current_socket = game_args->player_sockets[current_player_idx];
        int opponent_socket = game_args->player_sockets[1 - current_player_idx];
        char current_symbol = player_symbols[current_player_idx];
        char opponent_symbol = player_symbols[1 - current_player_idx];

        format_board_string_local(game_args, board_str); // board_str is BUFFER_SIZE
        // Send board (board_str is already a complete message here)
        send(game_args->player_sockets[0], board_str, strlen(board_str), 0);
        send(game_args->player_sockets[1], board_str, strlen(board_str), 0);

        snprintf(message_to_send, sizeof(message_to_send), "YOUR_TURN: Player %c, enter move (1-9) or 'chat <message>': \n", current_symbol);
        send(current_socket, message_to_send, strlen(message_to_send), 0);
        snprintf(message_to_send, sizeof(message_to_send), "WAIT: Player %c's (%s) turn.\n", current_symbol, game_args->player_ips[current_player_idx]);
        send(opponent_socket, message_to_send, strlen(message_to_send), 0);

        printf("INFO: Player %c's turn (%s:%d).\n", current_symbol, game_args->player_ips[current_player_idx], game_args->player_ports[current_player_idx]);

        int turn_action_taken = 0;
        while (!turn_action_taken && !game_over) {
            memset(buffer, 0, BUFFER_SIZE);
            bytes_received = recv(current_socket, buffer, BUFFER_SIZE - 1, 0);

            // If disconnected or error during game, notice to the opponent
            if (bytes_received <= 0) {
                printf("INFO: Player %c (%s:%d) disconnected.\n", current_symbol, game_args->player_ips[current_player_idx], game_args->player_ports[current_player_idx]);
                snprintf(message_to_send, sizeof(message_to_send), "GAME_OVER: OPPONENT_DISCONNECTED: Player %c disconnected. You win by default.\n", current_symbol);
                send(opponent_socket, message_to_send, strlen(message_to_send), 0);
                game_over = 1;
                pthread_mutex_lock(&stats_mutex);
                total_games_played++;
                if (opponent_symbol == 'X') x_wins++; else o_wins++;
                pthread_mutex_unlock(&stats_mutex);
                break;
            }
            buffer[bytes_received] = '\0';
            buffer[strcspn(buffer, "\n\r")] = 0;

            printf("DEBUG: Player %c (%s:%d) sent: '%s'\n", current_symbol, game_args->player_ips[current_player_idx], game_args->player_ports[current_player_idx], buffer);

            if (strncmp(buffer, "chat ", 5) == 0) {
                if (strlen(buffer + 5) > 0) {
                    char chat_msg_to_opponent[BUFFER_SIZE]; // Can use regular BUFFER_SIZE for this
                    snprintf(chat_msg_to_opponent, sizeof(chat_msg_to_opponent), "CHAT_MSG: [Player %c]: %s\n", current_symbol, buffer + 5);
                    send(opponent_socket, chat_msg_to_opponent, strlen(chat_msg_to_opponent), 0);
                    snprintf(message_to_send, sizeof(message_to_send), "INFO: Chat sent. YOUR_TURN: Player %c, enter move (1-9) or 'chat <message>': \n", current_symbol);
                    send(current_socket, message_to_send, strlen(message_to_send), 0);
                } else {
                    snprintf(message_to_send, sizeof(message_to_send), "INVALID_INPUT: Empty chat message. YOUR_TURN: Player %c, enter move (1-9) or 'chat <message>': \n", current_symbol);
                    send(current_socket, message_to_send, strlen(message_to_send), 0);
                }
            } else {
                int move_choice;
                if (sscanf(buffer, "%d", &move_choice) == 1) {
                    if (make_move_local(game_args, move_choice, current_symbol)) {
                        turn_count++;
                        turn_action_taken = 1;
                        snprintf(message_to_send, sizeof(message_to_send), "INFO: Move %d accepted.\n", move_choice);
                        send(current_socket, message_to_send, strlen(message_to_send), 0);
                    } else {
                        snprintf(message_to_send, sizeof(message_to_send), "INVALID_MOVE: Cell taken or invalid number. YOUR_TURN: Player %c, enter move (1-9) or 'chat <message>': \n", current_symbol);
                        send(current_socket, message_to_send, strlen(message_to_send), 0);
                    }
                } else {
                    snprintf(message_to_send, sizeof(message_to_send), "INVALID_INPUT: Enter a number (1-9) or 'chat <message>'. YOUR_TURN: Player %c, enter move (1-9) or 'chat <message>': \n", current_symbol);
                    send(current_socket, message_to_send, strlen(message_to_send), 0);
                }
            }
        }

        if (game_over) break;

        if (check_win_local(game_args, current_symbol)) {
            format_board_string_local(game_args, board_str);
            pthread_mutex_lock(&stats_mutex);
            total_games_played++;
            if (current_symbol == 'X') x_wins++; else o_wins++;
            // MODIFICATION HERE: Using sizeof(message_to_send) 
            snprintf(message_to_send, sizeof(message_to_send), "GAME_OVER: WINNER: Player %c wins!\n%s\nSTATS: Games: %d, X Wins: %d, O Wins: %d, Draws: %d\n",
                     current_symbol, board_str, total_games_played, x_wins, o_wins, draws);
            pthread_mutex_unlock(&stats_mutex);
            game_over = 1;
        } else if (turn_count == 9) {
             if (check_draw_local(game_args)) {
                format_board_string_local(game_args, board_str);
                pthread_mutex_lock(&stats_mutex);
                total_games_played++;
                draws++;
                // MODIFICATION HERE: Using sizeof(message_to_send)
                snprintf(message_to_send, sizeof(message_to_send), "GAME_OVER: DRAW: It's a draw!\n%s\nSTATS: Games: %d, X Wins: %d, O Wins: %d, Draws: %d\n",
                         board_str, total_games_played, x_wins, o_wins, draws);
                pthread_mutex_unlock(&stats_mutex);
                game_over = 1;
            }
        }

        if (game_over) {
            send(game_args->player_sockets[0], message_to_send, strlen(message_to_send), 0);
            send(game_args->player_sockets[1], message_to_send, strlen(message_to_send), 0);
            printf("INFO: Game ended. Result recorded.\n");
        } else {
            current_player_idx = 1 - current_player_idx;
        }
    }
    printf("INFO: Game thread finished for %s:%d and %s:%d.\n",
           game_args->player_ips[0], game_args->player_ports[0],
           game_args->player_ips[1], game_args->player_ports[1]);
}

// Thread function to handle a game session
void* game_thread_function(void* arg_ptr) {
    game_session_args_t* args = (game_session_args_t*)arg_ptr;

    play_tic_tac_toe(args);

    printf("INFO: Closing connections for game between %s:%d and %s:%d\n",
           args->player_ips[0], args->player_ports[0],
           args->player_ips[1], args->player_ports[1]);
    close(args->player_sockets[0]);
    close(args->player_sockets[1]);
    free(args);
    pthread_detach(pthread_self());
    return NULL;
}


int main() {
    int server_fd, new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_PENDING_CONNECTIONS) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        printf("INFO: Waiting for new connections...\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len)) < 0) {
            perror("accept failed");
            continue;
        }

        char client_ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, INET_ADDRSTRLEN);
        int client_port_val = ntohs(client_addr.sin_port);
        printf("INFO: Connection accepted from %s:%d\n", client_ip_str, client_port_val);

        pthread_mutex_lock(&matchmaking_mutex);
        if (waiting_player_socket == -1) {
            waiting_player_socket = new_socket;
            strcpy(waiting_player_ip, client_ip_str);
            waiting_player_port = client_port_val;
            pthread_mutex_unlock(&matchmaking_mutex);

            char msg_wait[BUFFER_SIZE]; // Regular buffer size is fine for this short message
            snprintf(msg_wait, sizeof(msg_wait), "INFO: You are Player X. Waiting for an opponent...\n");
            send(new_socket, msg_wait, strlen(msg_wait), 0);
            printf("INFO: Player X (%s:%d) is waiting.\n", client_ip_str, client_port_val);
        } else {
            game_session_args_t *args = malloc(sizeof(game_session_args_t));
            if (!args) {
                perror("Failed to allocate memory for game session args");
                close(new_socket);
                pthread_mutex_unlock(&matchmaking_mutex);
                continue;
            }

            args->player_sockets[0] = waiting_player_socket;
            strcpy(args->player_ips[0], waiting_player_ip);
            args->player_ports[0] = waiting_player_port;

            args->player_sockets[1] = new_socket;
            strcpy(args->player_ips[1], client_ip_str);
            args->player_ports[1] = client_port_val;

            waiting_player_socket = -1;
            pthread_mutex_unlock(&matchmaking_mutex);

            printf("INFO: Pairing Player X (%s:%d) with Player O (%s:%d).\n",
                   args->player_ips[0], args->player_ports[0],
                   args->player_ips[1], args->player_ports[1]);

            pthread_t game_tid;
            if (pthread_create(&game_tid, NULL, game_thread_function, args) != 0) {
                perror("pthread_create failed for game thread");
                close(args->player_sockets[0]);
                close(args->player_sockets[1]);
                free(args);
            }
        }
    }

    close(server_fd);
    return 0;
}