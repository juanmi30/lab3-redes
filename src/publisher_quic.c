// publisher_quic.c - Envía eventos al broker vía QUIC
// Ejecutar: publisher_quic.exe -target:127.0.0.1 -port:4433

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <msquic.h>

#define ALPN_STR "pubsub"
#define MAX_LINE 512

typedef struct {
    const QUIC_API_TABLE* Api;
    HQUIC Registration;
    HQUIC Configuration;
    HQUIC Connection;
    HQUIC Stream;
} App;

static void die(const char* m, QUIC_STATUS s){ fprintf(stderr,"%s (0x%x)\n",m,s); exit(1); }

/* helper: enviar con copia al heap */
static QUIC_STATUS send_copy(const QUIC_API_TABLE* Api, HQUIC Stream,
                             const void* data, uint32_t len) {
    uint8_t* mem = (uint8_t*)malloc(len);
    if (!mem) return QUIC_STATUS_OUT_OF_MEMORY;
    memcpy(mem, data, len);

    QUIC_BUFFER* qb = (QUIC_BUFFER*)malloc(sizeof(QUIC_BUFFER));
    if (!qb) { free(mem); return QUIC_STATUS_OUT_OF_MEMORY; }
    qb->Buffer = mem;
    qb->Length = len;

    return Api->StreamSend(Stream, qb, 1, 0, qb);
}

// Stream callback: mostrar respuestas del broker y liberar buffers
_Function_class_(QUIC_STREAM_CALLBACK)
QUIC_STATUS QUIC_API StreamCb(HQUIC Stream, void* Context, QUIC_STREAM_EVENT* Event) {
    switch (Event->Type) {
    case QUIC_STREAM_EVENT_RECEIVE:
        for (uint32_t i=0;i<Event->RECEIVE.BufferCount;++i) {
            fwrite(Event->RECEIVE.Buffers[i].Buffer, 1, Event->RECEIVE.Buffers[i].Length, stdout);
        }
        printf("\n");
        break;
    case QUIC_STREAM_EVENT_SEND_COMPLETE: {
        QUIC_BUFFER* qb = (QUIC_BUFFER*)Event->SEND_COMPLETE.ClientContext;
        if (qb) { free(qb->Buffer); free(qb); }
        break;
    }
    default: break;
    }
    return QUIC_STATUS_SUCCESS;
}

// Connection callback: una vez conectado abrimos un stream bidi
_Function_class_(QUIC_CONNECTION_CALLBACK)
QUIC_STATUS QUIC_API ConnCb(HQUIC Connection, void* Context, QUIC_CONNECTION_EVENT* Event) {
    App* app=(App*)Context;
    if (Event->Type==QUIC_CONNECTION_EVENT_CONNECTED) {
        if (QUIC_FAILED(app->Api->StreamOpen(Connection, QUIC_STREAM_OPEN_FLAG_NONE, StreamCb, app, &app->Stream)))
            return QUIC_STATUS_INTERNAL_ERROR;
        app->Api->StreamStart(app->Stream, QUIC_STREAM_START_FLAG_NONE);
        printf("Conectado y stream abierto. Formato: ARGvsBRA Gol al 32\n");
    }
    return QUIC_STATUS_SUCCESS;
}

int main(int argc, char** argv) {
    const char* target="127.0.0.1"; uint16_t port=4433;
    for (int i=1;i<argc;++i){
        if (!strncmp(argv[i],"-target:",8)) target=argv[i]+8;
        else if(!strncmp(argv[i],"-port:",6)) port=(uint16_t)atoi(argv[i]+6);
    }

    App app={0};
    QUIC_STATUS st=MsQuicOpen2(&app.Api); if(QUIC_FAILED(st)) die("MsQuicOpen2",st);
    QUIC_REGISTRATION_CONFIG reg={"pub-pubsub",QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    if (QUIC_FAILED(app.Api->RegistrationOpen(&reg,&app.Registration))) die("RegistrationOpen",st);

    QUIC_SETTINGS settings={0};
    settings.IsSet.PeerBidiStreamCount=TRUE; settings.PeerBidiStreamCount=1;
    QUIC_BUFFER alpn={sizeof(ALPN_STR)-1,(uint8_t*)ALPN_STR};
    if (QUIC_FAILED(app.Api->ConfigurationOpen(app.Registration,&alpn,1,&settings,sizeof(settings),NULL,&app.Configuration)))
        die("ConfigurationOpen",st);

    // Cliente sin validar cert (entorno de lab)
    QUIC_CREDENTIAL_CONFIG cred={0};
    cred.Type=QUIC_CREDENTIAL_TYPE_NONE;
    cred.Flags=QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    if(QUIC_FAILED(app.Api->ConfigurationLoadCredential(app.Configuration,&cred)))
        die("ConfigurationLoadCredential",st);

    if (QUIC_FAILED(app.Api->ConnectionOpen(app.Registration, ConnCb, &app, &app.Connection)))
        die("ConnectionOpen",st);

    if (QUIC_FAILED(app.Api->ConnectionStart(app.Connection, app.Configuration, QUIC_ADDRESS_FAMILY_UNSPEC, target, port)))
        die("ConnectionStart",st);

    char line[MAX_LINE];
    while (fgets(line,sizeof(line),stdin)) {
        if (!strncmp(line,"salir",5)) break;
        size_t n=strcspn(line,"\r\n"); line[n]='\0';
        // Asegurar que el stream ya existe (por si el usuario escribe muy rápido)
        while (app.Stream == NULL) Sleep(1);
        send_copy(app.Api, app.Stream, line, (uint32_t)strlen(line));
    }

    app.Api->StreamShutdown(app.Stream, QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL, 0);
    app.Api->ConnectionShutdown(app.Connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
    app.Api->ConfigurationClose(app.Configuration);
    app.Api->RegistrationClose(app.Registration);
    MsQuicClose(app.Api);
    return 0;
}
