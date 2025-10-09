#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define BROKER_IP "172.20.224.1"
#define BROKER_PORT 6000
#define BUF_SIZE 512

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;
    char buffer[BUF_SIZE];

    WSAStartup(MAKEWORD(2,2), &wsa);
    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(BROKER_IP);
    server.sin_port = htons(BROKER_PORT);

    connect(sock, (struct sockaddr*)&server, sizeof(server));
    printf("Publisher conectado al broker en %s:%d\n", BROKER_IP, BROKER_PORT);
    printf("Formato: ARGvsBRA Gol al minuto 32\n");

    while (1) {
        printf("Evento (o 'salir'): ");
        fgets(buffer, BUF_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "salir") == 0)
            break;

        send(sock, buffer, strlen(buffer), 0);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
