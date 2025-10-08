#include "mensaje.h"
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 5000

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in broker_addr;

    broker_addr.sin_family = AF_INET;
    broker_addr.sin_port = htons(PORT);
    broker_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // IP del broker

    Mensaje msg;
    msg.tipo = 'P';

    printf("=== PUBLISHER UDP ===\n");
    printf("Escriba 'salir' para cerrar o 'cambiar' para publicar a otro equipo.\n\n");

    while (1) {
        // Pregunta equipo
        printf("Ingrese el nombre del equipo: ");
        fgets(msg.equipo, sizeof(msg.equipo), stdin);
        msg.equipo[strcspn(msg.equipo, "\n")] = '\0'; // eliminar salto de línea

        if (strcmp(msg.equipo, "salir") == 0)
            break;

        printf("Publicando mensajes para el equipo '%s'.\n", msg.equipo);
        printf("Escriba 'cambiar' para elegir otro equipo.\n\n");

        while (1) {
            printf("> ");
            fgets(msg.contenido, sizeof(msg.contenido), stdin);
            msg.contenido[strcspn(msg.contenido, "\n")] = '\0';

            if (strcmp(msg.contenido, "salir") == 0) {
                closesocket(sock);
                WSACleanup();
                printf("Cerrando publisher...\n");
                return 0;
            }

            if (strcmp(msg.contenido, "cambiar") == 0) {
                printf("\n");
                break; // vuelve a preguntar por equipo
            }

            // Enviar al broker
            sendto(sock, (char*)&msg, sizeof(msg), 0,
                   (struct sockaddr*)&broker_addr, sizeof(broker_addr));

            printf("Mensaje enviado a [%s]: %s\n", msg.equipo, msg.contenido);
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
