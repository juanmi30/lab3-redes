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

    printf("Conectado al broker TCP en el puerto %d.\n", PORT);
    printf("Escriba 'salir' para terminar o 'cambiar' para publicar a otro equipo.\n");

    Mensaje msg;
    msg.tipo = 'P';

    while (1) {
        printf("Equipo: ");
        fgets(msg.equipo, sizeof(msg.equipo), stdin);
        msg.equipo[strcspn(msg.equipo, "\n")] = '\0';

        if (strcmp(msg.equipo, "salir") == 0)
            break;

        printf("Publicando para el equipo '%s'.\n", msg.equipo);

        while (1) {
            printf("> ");
            fgets(msg.contenido, sizeof(msg.contenido), stdin);
            msg.contenido[strcspn(msg.contenido, "\n")] = '\0';

            if (strcmp(msg.contenido, "salir") == 0) goto end;
            if (strcmp(msg.contenido, "cambiar") == 0) break;

            send(sock, (char*)&msg, sizeof(msg), 0);
            printf("Mensaje enviado a [%s]\n", msg.equipo);
        }
    }

end:
    closesocket(sock);
    WSACleanup();
    printf("Publisher cerrado.\n");
    return 0;
}
