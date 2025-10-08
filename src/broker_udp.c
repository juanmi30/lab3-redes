#include "mensaje.h"
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 5000
#define MAX_SUBS 100

typedef struct {
    char equipo[MAX_EQUIPO];
    struct sockaddr_in addr;
} Suscriptor;

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        printf("Error creando socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);
    Mensaje msg;
    Suscriptor subs[MAX_SUBS];
    int num_subs = 0;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Error en bind: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("Broker UDP escuchando en el puerto %d...\n", PORT);

    while (1) {
        recvfrom(sock, (char*)&msg, sizeof(msg), 0, (struct sockaddr*)&client_addr, &addr_len);

        if (msg.tipo == 'S') {
            subs[num_subs].addr = client_addr;
            strcpy(subs[num_subs].equipo, msg.equipo);
            num_subs++;
            printf("Nuevo suscriptor al equipo %s (%d total)\n", msg.equipo, num_subs);
        } 
        else if (msg.tipo == 'P') {
            printf("Publicacion recibida de equipo %s: %s\n", msg.equipo, msg.contenido);
            for (int i = 0; i < num_subs; i++) {
                if (strcmp(subs[i].equipo, msg.equipo) == 0) {
                    sendto(sock, (char*)&msg, sizeof(msg), 0,
                           (struct sockaddr*)&subs[i].addr, sizeof(subs[i].addr));
                }
            }
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
