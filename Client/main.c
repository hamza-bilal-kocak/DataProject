#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h> // Threading için gerekli

#pragma comment(lib, "ws2_32.lib") // Link Winsock library

#define BUFFER_SIZE 256

volatile int is_turn = 0; // Sıra kontrolü için paylaşılan değişken

// Sunucudan gelen mesajları dinleyen thread fonksiyonu
DWORD WINAPI receive_messages(LPVOID socket_desc) {
    SOCKET client_fd = *(SOCKET *)socket_desc;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int read_size = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (read_size <= 0) {
            printf("Server connection closed.\n");
            exit(1);
        }

        buffer[read_size] = '\0';
        printf("Server: %s\n", buffer);

        // Sıra kontrolü için "Your turn" mesajını kontrol et
        if (strstr(buffer, "Tahmin yapabilirsiniz!")) {
            is_turn = 1; // İstemciye tahmin yapma izni ver
        }

        // "Game Over" mesajı alındığında çıkış yap
        if (strstr(buffer, "Oyun bitti!")) {
            break;
        }
    }

    return 0;
}

int main() {
    WSADATA wsa;
    SOCKET client_fd;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE] = {0};

    // Winsock başlat
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Winsock init failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Socket oluştur
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8080);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Sunucuya bağlan
    if (connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("Connection failed. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    // Mesajları dinleyen thread başlat
    HANDLE recv_thread = CreateThread(
        NULL,                 // Default security attributes
        0,                    // Default stack size
        receive_messages,     // Thread function
        (LPVOID)&client_fd,   // Thread parameter (socket_fd)
        0,                    // Default creation flags
        NULL                  // No thread ID
    );

    if (recv_thread == NULL) {
        printf("Thread creation failed. Error Code: %d\n", GetLastError());
        return 1;
    }

    // Ana döngü
    while (1) {
        if (is_turn) {
            printf("Enter your guess (1-100): ");
            fgets(buffer, BUFFER_SIZE, stdin);
            buffer[strcspn(buffer, "\n")] = 0; // Yeni satır karakterini kaldır

            // Tahminin geçerli bir sayı olup olmadığını kontrol et
            if (atoi(buffer) <= 0 || atoi(buffer) > 100) {
                printf("Invalid input. Please enter a number between 1 and 100.\n");
                continue;
            }

            // Sunucuya tahmini gönder
            if (send(client_fd, buffer, strlen(buffer), 0) == SOCKET_ERROR) {
                printf("Send failed. Error Code: %d\n", WSAGetLastError());
                break;
            }

            is_turn = 0; // Tahmin yaptıktan sonra sırasını beklemeye al
        }
    }

    // Kapanmak için tuşa basmayı bekle
    printf("Kapatmak icin Istemcileri kapatiniz...\n");

    // Burada klavyeden bir tuş almayı bekliyoruz (getchar kullanarak)
    getchar();  // Tek bir tuş basımı bekler

    // Kaynakları temizle
    WaitForSingleObject(recv_thread, INFINITE);
    CloseHandle(recv_thread);
    closesocket(client_fd);
    WSACleanup();

    return 0;
}
