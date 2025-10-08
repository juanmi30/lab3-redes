#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define BROKER_IP "127.0.0.1"
#define BROKER_PORT 6000
#define BUF_SIZE 512

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;
    char buffer[BUF_SIZE];
    int recv_len;

    WSAStartup(MAKEWORD(2,2), &wsa);
    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(BROKER_IP);
    server.sin_port = htons(BROKER_PORT);

    connect(sock, (struct sockaddr*)&server, sizeof(server));

    printf("Ingrese partidos a suscribirse (ej: ARGvsBRA COLvsURU): ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = 0;

    char mensaje[BUF_SIZE];
    sprintf(mensaje, "SUB %s", buffer);
    send(sock, mensaje, strlen(mensaje), 0);

    printf("Esperando confirmacion...\n");

    recv_len = recv(sock, buffer, BUF_SIZE - 1, 0);
    buffer[recv_len] = '\0';
    printf("%s\n", buffer);
    printf("Esperando actualizaciones...\n");

    while ((recv_len = recv(sock, buffer, BUF_SIZE - 1, 0)) > 0) {
        buffer[recv_len] = '\0';
        printf("%s\n", buffer);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
