#include "mensaje.h"
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 6000
#define MAX_CLIENTS 50

typedef struct {
    SOCKET socket;
    char equipo[MAX_EQUIPO];
    int es_suscriptor;
} Cliente;

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        printf("Error al crear socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);
    Cliente clientes[MAX_CLIENTS];
    int num_clientes = 0;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listener, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Error en bind: %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    listen(listener, 5);
    printf("Broker TCP escuchando en puerto %d...\n", PORT);

    fd_set readfds;
    Mensaje msg;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listener, &readfds);

        SOCKET max_sd = listener;

        for (int i = 0; i < num_clientes; i++) {
            FD_SET(clientes[i].socket, &readfds);
            if (clientes[i].socket > max_sd)
                max_sd = clientes[i].socket;
        }

        if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) {
            printf("Error en select: %d\n", WSAGetLastError());
            break;
        }

        // Nueva conexión
        if (FD_ISSET(listener, &readfds)) {
            SOCKET new_sock = accept(listener, (struct sockaddr*)&client_addr, &addr_len);
            if (new_sock != INVALID_SOCKET) {
                clientes[num_clientes].socket = new_sock;
                clientes[num_clientes].es_suscriptor = 0;
                strcpy(clientes[num_clientes].equipo, "");
                num_clientes++;
                printf("Nuevo cliente conectado (%d total)\n", num_clientes);
            }
        }

        // Actividad en clientes existentes
        for (int i = 0; i < num_clientes; i++) {
            SOCKET s = clientes[i].socket;
            if (FD_ISSET(s, &readfds)) {
                int bytes = recv(s, (char*)&msg, sizeof(msg), 0);
                if (bytes <= 0) {
                    printf("Cliente desconectado\n");
                    closesocket(s);
                    clientes[i] = clientes[num_clientes - 1];
                    num_clientes--;
                    i--;
                } else {
                    if (msg.tipo == 'S') {
                        clientes[i].es_suscriptor = 1;
                        strcpy(clientes[i].equipo, msg.equipo);
                        printf("Nuevo suscriptor al equipo %s\n", msg.equipo);
                    } else if (msg.tipo == 'P') {
                        printf("Publicación de [%s]: %s\n", msg.equipo, msg.contenido);
                        for (int j = 0; j < num_clientes; j++) {
                            if (clientes[j].es_suscriptor &&
                                strcmp(clientes[j].equipo, msg.equipo) == 0) {
                                send(clientes[j].socket, (char*)&msg, sizeof(msg), 0);
                            }
                        }
                    }
                }
            }
        }
    }

    closesocket(listener);
    WSACleanup();
    return 0;
}
