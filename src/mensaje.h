#ifndef MENSAJE_H
#define MENSAJE_H

#define MAX_EQUIPO 50
#define MAX_MENSAJE 256

typedef struct {
    char tipo;              // 'S' = suscripción, 'P' = publicación
    char equipo[MAX_EQUIPO];
    char contenido[MAX_MENSAJE];  // solo usado en publicaciones
} Mensaje;

#endif
