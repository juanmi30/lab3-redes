#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define BROKER_PORT 6000
#define MAX_CLIENTS 20
#define MAX_MATCHES_PER_SUB 10
#define BUF_SIZE 512

typedef struct {
    SOCKET socket;
    int active;
    int is_subscriber;
    char matches[MAX_MATCHES_PER_SUB][50];
    int match_count;
} Client;

int is_match_in_message(const char *message, const char *match) {
    return strstr(message, match) != NULL;
}

int main() {
    WSADATA wsa;
    SOCKET listener, new_socket;
    struct sockaddr_in server, client;
    int c, activity, i, valread;
    fd_set readfds;
    char buffer[BUF_SIZE];
    Client clients[MAX_CLIENTS] = {0};

    WSAStartup(MAKEWORD(2,2), &wsa);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(BROKER_PORT);

    bind(listener, (struct sockaddr*)&server, sizeof(server));
    listen(listener, 5);
    printf("Broker TCP activo en puerto %d\n", BROKER_PORT);

    c = sizeof(struct sockaddr_in);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listener, &readfds);
        SOCKET max_sd = listener;

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                FD_SET(clients[i].socket, &readfds);
                if (clients[i].socket > max_sd)
                    max_sd = clients[i].socket;
            }
        }

        activity = select(0, &readfds, NULL, NULL, NULL);

        // Nueva conexión
        if (FD_ISSET(listener, &readfds)) {
            new_socket = accept(listener, (struct sockaddr*)&client, &c);
            printf("Nueva conexion: %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

            for (i = 0; i < MAX_CLIENTS; i++) {
                if (!clients[i].active) {
                    clients[i].socket = new_socket;
                    clients[i].active = 1;
                    clients[i].is_subscriber = 0;
                    break;
                }
            }
        }

        // Mensajes de clientes
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active && FD_ISSET(clients[i].socket, &readfds)) {
                valread = recv(clients[i].socket, buffer, BUF_SIZE - 1, 0);
                if (valread <= 0) {
                    closesocket(clients[i].socket);
                    clients[i].active = 0;
                    continue;
                }
                buffer[valread] = '\0';

                if (strncmp(buffer, "SUB ", 4) == 0) {
                    clients[i].is_subscriber = 1;
                    clients[i].match_count = 0;

                    char *token = strtok(buffer + 4, " ");
                    while (token && clients[i].match_count < MAX_MATCHES_PER_SUB) {
                        strcpy(clients[i].matches[clients[i].match_count++], token);
                        token = strtok(NULL, " ");
                    }

                    printf("Suscriptor conectado: socket → %d\n", clients[i].socket);
                    for (int t = 0; t < clients[i].match_count; t++)
                        printf("%s ", clients[i].matches[t]);
                    printf("\n");

                    char confirm[BUF_SIZE] = "Suscripcion confirmada a: ";
                    for (int t = 0; t < clients[i].match_count; t++) {
                        strcat(confirm, clients[i].matches[t]);
                        strcat(confirm, " ");
                    }
                    send(clients[i].socket, confirm, strlen(confirm), 0);

                } else {
                    printf("Mensaje publisher: %s\n", buffer);

                    // Reenviar solo a los suscriptores interesados
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].active && clients[j].is_subscriber) {
                            for (int t = 0; t < clients[j].match_count; t++) {
                                if (is_match_in_message(buffer, clients[j].matches[t])) {
                                    send(clients[j].socket, buffer, strlen(buffer), 0);
                                    break;
                                }
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
