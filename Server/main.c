#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <time.h>
#include <ctype.h>

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

#define BUFFER_SIZE 256
#define MAX_ATTEMPTS 10 // Maximum attempts allowed for each player

// Function to check if a string is an integer
int is_integer(const char *str) {
    if (*str == '-' || *str == '+') str++;
    while (*str) {
        if (!isdigit(*str)) return 0;
        str++;
    }
    return 1;
}

int main() {
    WSADATA wsa;
    SOCKET server_fd, client_sockets[2];
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    int client_count = 0;
    int target = 0;
    int current_turn = 0; // Keeps track of which client's turn it is

    int wrong_guesses[2] = {0};  // To count wrong guesses of each client
    int total_guesses[2] = {0};  // To track the number of guesses for each client
    char wrong_guesses_list[2][MAX_ATTEMPTS][BUFFER_SIZE]; // Store wrong guesses for each client
    int correct_guess_turn[2] = {-1, -1}; // Store the turn at which each client guesses correctly, -1 if not guessed yet
    int game_over = 0; // Flag to check if the game is over

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // Bind
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Bind failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Listen
    if (listen(server_fd, 2) == SOCKET_ERROR) {
        printf("Listen failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    printf("Server listening...\n");

    // Generate target number
    srand((unsigned int)time(NULL));
    target = rand() % 100 + 1;
    printf("Tahmin edilmesi gereken sayi: %d\n", target);

    // Accept two clients
    while (client_count < 2) {
        SOCKET new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket == INVALID_SOCKET) {
            printf("Accept failed. Error Code: %d\n", WSAGetLastError());
            continue;
        }

        client_sockets[client_count] = new_socket;
        printf("Yeni baglanti kuruldu... Istemci #%d\n", client_count + 1);

        // Send welcome message
        sprintf(buffer, "Hos geldiniz Istemci #%d! Oyuna baslamak icin bekleyiniz.\n", client_count + 1);
        send(new_socket, buffer, strlen(buffer), 0);

        client_count++;
    }

    // Notify clients to start
    for (int i = 0; i < 2; i++) {
        sprintf(buffer, "Oyun basliyor! Tahmin yapma sirasi: Istemci #1\n");
        send(client_sockets[i], buffer, strlen(buffer), 0);
    }

    // Game loop
    while (!game_over) {
        SOCKET current_client = client_sockets[current_turn];
        SOCKET other_client = client_sockets[1 - current_turn];

        // Notify current client to make a guess
        sprintf(buffer, "Tahmin yapabilirsiniz!\n");
        send(current_client, buffer, strlen(buffer), 0);

        // Notify the other client to wait
        sprintf(buffer, "Siranizi bekleyiniz.\n");
        send(other_client, buffer, strlen(buffer), 0);

        // Receive guess from current client
        int read_size = recv(current_client, buffer, BUFFER_SIZE - 1, 0);
        if (read_size <= 0) {
            printf("Istemci #%d baglantisi kapandi.\n", current_turn + 1);
            break;
        }

        buffer[read_size] = '\0';
        int guess = atoi(buffer);
        total_guesses[current_turn]++; // Increment the number of guesses for the current client
        printf("Istemci #%d tahmin etti: %d\n", current_turn + 1, guess);

        // Check the guess
        if (guess == target) {
            correct_guess_turn[current_turn] = total_guesses[current_turn]; // Store the turn in which the client guessed correctly
            sprintf(buffer, "Dogru tahmin! Tebrikler Istemci #%d kazandi!\n", current_turn + 1);
            send(current_client, buffer, strlen(buffer), 0);

            sprintf(buffer, "Oyun bitti! Istemci #%d kazandi!\n", current_turn + 1);
            send(other_client, buffer, strlen(buffer), 0);

            game_over = 1;
        } else if (guess < target) {
            sprintf(buffer, "Daha buyuk bir sayi deneyin.\n");
            send(current_client, buffer, strlen(buffer), 0);
            wrong_guesses[current_turn]++; // Increment the wrong guesses count for the current client
            sprintf(wrong_guesses_list[current_turn][wrong_guesses[current_turn] - 1], "%d", guess); // Store wrong guess
        } else {
            sprintf(buffer, "Daha kucuk bir sayi deneyin.\n");
            send(current_client, buffer, strlen(buffer), 0);
            wrong_guesses[current_turn]++; // Increment the wrong guesses count for the current client
            sprintf(wrong_guesses_list[current_turn][wrong_guesses[current_turn] - 1], "%d", guess); // Store wrong guess
        }

        // Switch turn
        current_turn = 1 - current_turn;
    }

    // Print statistics after the game ends and send to both clients
    for (int i = 0; i < 2; i++) {
        if (i == current_turn) {
            printf("Istemci #%d kazandi ve %d. tahminde dogru cevabi buldu.\n", i + 1, correct_guess_turn[i]);
        } else {
            printf("Istemci #%d kaybetti ve %d hatali tahmin yapti.\n", i + 1, wrong_guesses[i]);
        }

        // Send the result to both clients: correct guess info and wrong guesses list of opponent
        sprintf(buffer, "Oyun Bitti! Istemci #%d kazandi!\n", current_turn + 1);
        send(client_sockets[i], buffer, strlen(buffer), 0);

        // Show wrong guesses of the opponent
        sprintf(buffer, "Rakibinizin yaptigi hatali tahminler: ");
        for (int j = 0; j < wrong_guesses[1 - i]; j++) {
            char wrong_guess[BUFFER_SIZE];
            sprintf(wrong_guess, "%s ", wrong_guesses_list[1 - i][j]);
            strcat(buffer, wrong_guess);
        }
        send(client_sockets[i], buffer, strlen(buffer), 0);
    }

    // Wait for any client to press a key to close the terminals
    printf("Istemcilerden birinin klavyeden bir tus basmasi bekleniyor...\n");
    char exit_buffer[1];
    int exit_flag = 0;
    while (!exit_flag) {
        for (int i = 0; i < 2; i++) {
            if (recv(client_sockets[i], exit_buffer, sizeof(exit_buffer), 0) > 0) {
                printf("Istemci #%d kapaniyor.\n", i + 1);
                exit_flag = 1;
                break;
            }
        }
    }

    // Close connections
    for (int i = 0; i < 2; i++) {
        closesocket(client_sockets[i]);
    }
    closesocket(server_fd);
    WSACleanup();

    return 0;
}
