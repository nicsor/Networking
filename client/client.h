#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NAME_LEN 64

typedef enum
{
	E_OK,
	E_CONNECTION_FAILURE,
	E_NOT_MANAGED
} Status;

typedef uint16_t ServerId;
typedef void * ClientHandler;
typedef void (*notify_cb)(ClientHandler handler, char *buffer, int size);

typedef struct  {
	char * ip[16];
	uint16_t port;
    char name[MAX_NAME_LEN];
    notify_cb receiveCallback;
} ServerConfig;

typedef struct  {
	ServerId ID;
    char name[MAX_NAME_LEN];
} ServerInfo;

ClientHandler client_init(ServerConfig * config);
Status client_deinit(ClientHandler handler);
Status client_connect(ClientHandler handler, ServerId id);
Status client_disconnect(ClientHandler handler);
uint16_t client_list_servers(ClientHandler handler, ServerInfo *servers, uint16_t maxServerCount, uint16_t timeoutSec);
Status client_send_message(ClientHandler handler, char * buffer, ssize_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif /* CLIENT_H_*/