// broker_quic.c - Broker Pub/Sub con QUIC (MsQuic) - Windows
// Compilar con: msquic.lib ntdll.lib ws2_32.lib
// Ejecutar: broker_quic.exe -cert_hash:HEXTHUMBPRINT [-port:4433]

// Debe ser lo PRIMERO del archivo
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <msquic.h>

#define MAX_SUBS     32
#define MAX_MATCHES  16
#define MAX_LINE     512
#define ALPN_STR     "pubsub"
#define DEFAULT_PORT 4433

typedef struct {
    HQUIC Conn;
    HQUIC Stream;            // stream bidireccional con el subscriptor
    int   active;
    int   id;                // <-- NUEVO: identificador simple para logs
    int   match_count;
    char  matches[MAX_MATCHES][48];
} Subscriber;

typedef struct {
    const QUIC_API_TABLE* Api;
    HQUIC Registration;
    HQUIC Configuration;
    HQUIC Listener;
    uint16_t Port;
    Subscriber subs[MAX_SUBS];
} App;

/* contador simple para asignar IDs a suscriptores en los logs */
static int g_next_sub_id = 1;

static int msg_has_match(const char* msg, const Subscriber* s) {
    for (int i = 0; i < s->match_count; ++i) {
        if (strstr(msg, s->matches[i]) != NULL) return 1;
    }
    return 0;
}

/* -------- helper: enviar copiando al heap y liberar en SEND_COMPLETE -------- */
static QUIC_STATUS send_copy(const QUIC_API_TABLE* Api, HQUIC Stream,
                             const void* data, uint32_t len) {
    uint8_t* mem = (uint8_t*)malloc(len);
    if (!mem) return QUIC_STATUS_OUT_OF_MEMORY;
    memcpy(mem, data, len);

    QUIC_BUFFER* qb = (QUIC_BUFFER*)malloc(sizeof(QUIC_BUFFER));
    if (!qb) { free(mem); return QUIC_STATUS_OUT_OF_MEMORY; }
    qb->Buffer = mem;
    qb->Length = len;

    // Pasamos qb como ClientContext para poder liberarlo al completar el envío.
    return Api->StreamSend(Stream, qb, 1, 0, qb);
}

/* util: volcar lista de tópicos en una cadena para log */
static void topics_to_string(const Subscriber* s, char* out, size_t outsz) {
    out[0] = '\0';
    for (int i = 0; i < s->match_count; ++i) {
        strncat(out, s->matches[i], outsz - strlen(out) - 1);
        if (i + 1 < s->match_count) strncat(out, " ", outsz - strlen(out) - 1);
    }
}

/* ---------- Callbacks de Stream (del lado servidor) ---------- */
_IRQL_requires_max_(DISPATCH_LEVEL)
_Function_class_(QUIC_STREAM_CALLBACK)
QUIC_STATUS QUIC_API StreamCb(HQUIC Stream, void* Context, QUIC_STREAM_EVENT* Event) {
    App* app = (App*)Context;
    switch (Event->Type) {
    case QUIC_STREAM_EVENT_RECEIVE: {
        // Llega una línea del cliente (SUB ... o evento de publisher)
        for (uint32_t i = 0; i < Event->RECEIVE.BufferCount; ++i) {
            const QUIC_BUFFER* qb = &Event->RECEIVE.Buffers[i];
            char buf[MAX_LINE+1] = {0};
            size_t n = qb->Length < MAX_LINE ? qb->Length : MAX_LINE;
            memcpy(buf, qb->Buffer, n);
            buf[n] = '\0';

            // ¿Quién envió? Buscamos si este Stream corresponde a un subscriptor ya registrado
            Subscriber* who = NULL;
            for (int s = 0; s < MAX_SUBS; ++s) {
                if (app->subs[s].active && app->subs[s].Stream == Stream) { who = &app->subs[s]; break; }
            }

            if (strncmp(buf, "SUB ", 4) == 0) {
                // Registrar/actualizar suscriptor
                if (!who) { // buscar slot libre
                    for (int s = 0; s < MAX_SUBS; ++s) if (!app->subs[s].active) { who = &app->subs[s]; break; }
                    if (who) {
                        who->active = 1;
                        who->id = g_next_sub_id++;          // asignar ID para logs
                        who->Conn = NULL; // se fija en ConnectionCb
                        who->Stream = Stream;
                    }
                }
                if (who) {
                    who->match_count = 0;
                    char* p = buf + 4;
                    char* tok = strtok(p, " ");
                    while (tok && who->match_count < MAX_MATCHES) {
                        strncpy(who->matches[who->match_count++], tok, 47);
                        tok = strtok(NULL, " ");
                    }

                    char topics[256]; topics_to_string(who, topics, sizeof(topics));
                    printf("[BROKER] SUB de cliente #%d (stream=%p) -> %d tópicos: %s\n",
                           who->id, (void*)who->Stream, who->match_count, topics);

                    // Confirmación
                    char ok[256] = "Suscripcion OK: ";
                    strncat(ok, topics, sizeof(ok) - strlen(ok) - 1);
                    send_copy(app->Api, Stream, ok, (uint32_t)strlen(ok));
                }
            } else {
                // Es un evento de publisher -> log y reenviar a subs con match
                printf("[BROKER] Evento entrante: \"%s\"\n", buf);

                int enviados = 0;
                char a_quienes[256] = {0};

                for (int s = 0; s < MAX_SUBS; ++s) {
                    if (app->subs[s].active && app->subs[s].Stream) {
                        if (msg_has_match(buf, &app->subs[s])) {
                            send_copy(app->Api, app->subs[s].Stream, buf, (uint32_t)strlen(buf));
                            // acumular IDs para el resumen
                            char tmp[16];
                            _snprintf(tmp, sizeof(tmp), "%s#%d",
                                      (enviados ? "," : ""), app->subs[s].id);
                            strncat(a_quienes, tmp, sizeof(a_quienes) - strlen(a_quienes) - 1);
                            ++enviados;
                        }
                    }
                }

                printf("[BROKER] Reenvío: %d suscriptor(es) %s\n",
                       enviados, (enviados ? a_quienes : "(ninguno)"));
            }
        }
        break;
    }
    case QUIC_STREAM_EVENT_SEND_COMPLETE: {
        // Liberar los buffers del heap usados en send_copy
        QUIC_BUFFER* qb = (QUIC_BUFFER*)Event->SEND_COMPLETE.ClientContext;
        if (qb) { free(qb->Buffer); free(qb); }
        break;
    }
    case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN: /* ignore */ break;
    case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE: { /* stream cerrado; marcar inactivo si era sub */
        for (int s = 0; s < MAX_SUBS; ++s) {
            if (app->subs[s].active && app->subs[s].Stream == Stream) {
                printf("[BROKER] Stream cerrado de cliente #%d (stream=%p)\n",
                       app->subs[s].id, (void*)Stream);
                app->subs[s].active = 0;
                app->subs[s].Stream = NULL;
                app->subs[s].Conn = NULL;
            }
        }
        break;
    }
    default: break;
    }
    return QUIC_STATUS_SUCCESS;
}

/* ---------- Callback de Conexión ---------- */
_IRQL_requires_max_(DISPATCH_LEVEL)
_Function_class_(QUIC_CONNECTION_CALLBACK)
QUIC_STATUS QUIC_API ConnectionCb(HQUIC Connection, void* Context, QUIC_CONNECTION_EVENT* Event) {
    App* app = (App*)Context;
    switch (Event->Type) {
    case QUIC_CONNECTION_EVENT_CONNECTED:
        printf("[BROKER] Conexión establecida (%p)\n", (void*)Connection);
        break;
    case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED: {
        HQUIC Stream = Event->PEER_STREAM_STARTED.Stream;
        app->Api->SetCallbackHandler(Stream, (void*)StreamCb, app);
        printf("[BROKER] Stream del peer iniciado (%p)\n", (void*)Stream);
        break;
    }
    case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
        printf("[BROKER] Conexión cerrada (%p)\n", (void*)Connection);
        // Marcar subs inactivos que dependían de esta conexión
        for (int s = 0; s < MAX_SUBS; ++s) {
            if (app->subs[s].active && app->subs[s].Conn == Connection) {
                app->subs[s].active = 0;
                app->subs[s].Stream = NULL;
                app->subs[s].Conn = NULL;
            }
        }
        break;
    default: break;
    }
    return QUIC_STATUS_SUCCESS;
}

/* ---------- Callback del Listener (nuevas conexiones) ---------- */
_IRQL_requires_max_(DISPATCH_LEVEL)
_Function_class_(QUIC_LISTENER_CALLBACK)
QUIC_STATUS QUIC_API ListenerCb(HQUIC Listener, void* Context, QUIC_LISTENER_EVENT* Event) {
    App* app = (App*)Context;
    if (Event->Type == QUIC_LISTENER_EVENT_NEW_CONNECTION) {
        HQUIC Conn = Event->NEW_CONNECTION.Connection;
        app->Api->SetCallbackHandler(Conn, (void*)ConnectionCb, app);
        app->Api->ConnectionSetConfiguration(Conn, app->Configuration);
        printf("[BROKER] Nueva conexión entrante (%p)\n", (void*)Conn);
    }
    return QUIC_STATUS_SUCCESS;
}

static void die(const char* msg, QUIC_STATUS st) {
    fprintf(stderr, "%s (0x%x)\n", msg, st);
    exit(1);
}

int main(int argc, char** argv) {
    App app = {0};
    app.Port = DEFAULT_PORT;

    // Parse args básicos: -cert_hash:HEX   -port:N
    const char* cert_hash = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "-cert_hash:", 11) == 0) cert_hash = argv[i] + 11;
        else if (strncmp(argv[i], "-port:", 6) == 0) app.Port = (uint16_t)atoi(argv[i] + 6);
    }
    if (!cert_hash) {
        fprintf(stderr, "Uso: %s -cert_hash:THUMBPRINT [-port:4433]\n", argv[0]);
        return 2;
    }

    QUIC_STATUS st = MsQuicOpen2(&app.Api);
    if (QUIC_FAILED(st)) die("MsQuicOpen2 fallo", st);

    QUIC_REGISTRATION_CONFIG regCfg = { "pubsub-broker", QUIC_EXECUTION_PROFILE_LOW_LATENCY };
    if (QUIC_FAILED(app.Api->RegistrationOpen(&regCfg, &app.Registration))) die("RegistrationOpen", st);

    // ALPN
    QUIC_BUFFER alpn = { sizeof(ALPN_STR)-1, (uint8_t*)ALPN_STR };

    QUIC_SETTINGS settings = {0};
    settings.IdleTimeoutMs = 60000; settings.IsSet.IdleTimeoutMs = TRUE;
    settings.PeerBidiStreamCount = 64; settings.IsSet.PeerBidiStreamCount = TRUE;

    if (QUIC_FAILED(app.Api->ConfigurationOpen(app.Registration, &alpn, 1, &settings, sizeof(settings), NULL, &app.Configuration)))
        die("ConfigurationOpen", st);

    // Cargar credenciales (certificado por hash del almacén CurrentUser\My)
    QUIC_CREDENTIAL_CONFIG cred = {0};
    cred.Type  = QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH;
    cred.Flags = QUIC_CREDENTIAL_FLAG_NONE | QUIC_CREDENTIAL_FLAG_ENABLE_OCSP;
    QUIC_CERTIFICATE_HASH hash = {0};
    size_t len = strlen(cert_hash);
    for (size_t i = 0; i < len/2 && i < 20; ++i) {
        unsigned int byte;
        sscanf(cert_hash + (i*2), "%02x", &byte);
        hash.ShaHash[i] = (uint8_t)byte;
    }
    cred.CertificateHash = &hash;

    if (QUIC_FAILED(app.Api->ConfigurationLoadCredential(app.Configuration, &cred)))
        die("ConfigurationLoadCredential", st);

    if (QUIC_FAILED(app.Api->ListenerOpen(app.Registration, ListenerCb, &app, &app.Listener)))
        die("ListenerOpen", st);

    QUIC_ADDR addr = {0};
    QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_UNSPEC);
    QuicAddrSetPort(&addr, app.Port);
    if (QUIC_FAILED(app.Api->ListenerStart(app.Listener, &alpn, 1, &addr)))
        die("ListenerStart", st);

    printf("Broker QUIC en puerto %u (ALPN=%s)\n", app.Port, ALPN_STR);
    printf("Listo. Mantén la ventana abierta (Ctrl+C para salir)\n");
    getchar();

    app.Api->ListenerStop(app.Listener);
    app.Api->ListenerClose(app.Listener);
    app.Api->ConfigurationClose(app.Configuration);
    app.Api->RegistrationClose(app.Registration);
    MsQuicClose(app.Api);
    return 0;
}
