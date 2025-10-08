#include "mensaje.h"
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 6000

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in broker_addr;

    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(PORT);
    broker_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&broker_addr, sizeof(broker_addr)) == SOCKET_ERROR) {
        printf("No se pudo conectar al broker.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    Mensaje msg;
    msg.tipo = 'S';

    printf("Ingrese el equipo al que desea suscribirse: ");
    fgets(msg.equipo, sizeof(msg.equipo), stdin);
    msg.equipo[strcspn(msg.equipo, "\n")] = '\0';

    send(sock, (char*)&msg, sizeof(msg), 0);
    printf("Suscrito al equipo '%s'. Esperando mensajes...\n", msg.equipo);

    while (1) {
        int bytes = recv(sock, (char*)&msg, sizeof(msg), 0);
        if (bytes <= 0) {
            printf("Conexión cerrada por el broker.\n");
            break;
        }
        printf("[Mensaje de %s] %s\n", msg.equipo, msg.contenido);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
