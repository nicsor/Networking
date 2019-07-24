#include "client.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

typedef struct
{
    in_addr_t ip;
    uint16_t port;
} Server_t;

typedef struct
{
    char * ip[16];
    uint16_t port;
    uint16_t max_nb_servers;
    Status status;
    int socket_fd;
    notify_cb receive_cb;
    notify_cb_error error_cb;
    pthread_t receive_thread;
    Server_t *detected_servers;
    int detected_servers_count;
} ClientInfo;

typedef ClientInfo * ClientHandler_t;

ClientHandler client_init(ClientConfig * config)
{
    ClientHandler_t handler = (ClientHandler_t) malloc (sizeof(ClientInfo));
    memset (handler, 0, sizeof(ClientInfo));

    if (handler == NULL)
    {
        return NULL;
    }

    memcpy (handler->ip, config->ip, sizeof(handler->ip));
    handler->port = config->port;
    handler->status = 0;
    handler->receive_cb = config->receiveCallback;
    handler->max_nb_servers = config->max_nb_servers;

    ssize_t sizeofServerData = sizeof(Server_t) * handler->max_nb_servers;
    handler->detected_servers = (Server_t*) malloc (sizeofServerData);

    if (handler->detected_servers == NULL)
    {
        free (handler);
        return NULL;
    }

    memset (handler->detected_servers, 0, sizeofServerData);

    return handler;
}

void client_deinit(ClientHandler handler)
{
    client_disconnect (handler);
    free (handler);
}

uint16_t client_list_servers(
    ClientHandler param,
    ServerInfo *servers,
    uint16_t maxServerCount,
    uint16_t timeoutSec)
{
    ClientHandler_t handler = (ClientHandler_t) param;

    struct sockaddr_in addr;
    int addrlen, sock, status;
    char message[200];

    /* set up socket */
    sock = socket (AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        return 0;
    }
    memset (&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr ((const char *) handler->ip);
    addr.sin_port = htons (handler->port);
    addrlen = sizeof(addr);

    struct timeval tv;
    tv.tv_sec = timeoutSec;
    tv.tv_usec = 0;
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

    status = sendto (sock, message, sizeof(message), 0, (struct sockaddr *) &addr, addrlen);

    if (status < 0)
    {
        close (sock);
        return 0;
    }

    handler->detected_servers_count = 0;

    int nrMaximServere =
        (maxServerCount > handler->max_nb_servers) ? handler->max_nb_servers : maxServerCount;

    while (handler->detected_servers_count < nrMaximServere)
    {
        int bytesRcvd = recvfrom (
            sock,
            (void *) message,
            sizeof(message),
            (int) 0,
            (struct sockaddr *) &addr,
            (unsigned int *) &addrlen);

        if (bytesRcvd < 0)
        {

            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                // timeout
                break;
            }
            else
            {
                close (sock);
                return handler->detected_servers_count;
            }
        }

        printf ("Recived %d bytes", bytesRcvd);

        servers[handler->detected_servers_count].ID = handler->detected_servers_count;
        memcpy (servers[handler->detected_servers_count].name, message, 20);
        memcpy (
            &handler->detected_servers[handler->detected_servers_count].port,
            &message[20],
            sizeof(handler->detected_servers[handler->detected_servers_count].port));

        handler->detected_servers[handler->detected_servers_count].ip = addr.sin_addr.s_addr;
        handler->detected_servers[handler->detected_servers_count].port = ntohs (
            handler->detected_servers[handler->detected_servers_count].port);

        printf (
            "Server detected: %s %d\n",
            servers[handler->detected_servers_count].name,
            handler->detected_servers[handler->detected_servers_count].port);

        ++handler->detected_servers_count;
    }

    close (sock);

    return handler->detected_servers_count;
}

void *recive_thread(void *param)
{
    char buffer[1024];
    ClientHandler_t handler = (ClientHandler_t) param;

    while (1)
    {
        int dataLength = recv (handler->socket_fd, buffer, sizeof(buffer), 0);

        if (dataLength <= 0)
        {
            break;
        }

        handler->receive_cb (handler, buffer, dataLength);
    }

    client_disconnect (handler);

    return NULL;
}

Status client_connect(ClientHandler param, ServerId id)
{
    ClientHandler_t handler = (ClientHandler_t) param;

    if (id < handler->detected_servers_count)
    {
        struct sockaddr_in ra;

        handler->socket_fd = socket (PF_INET, SOCK_STREAM, 0);

        if (handler->socket_fd < 0)
        {
            handler->socket_fd = 0;
            return E_CONNECTION_FAILURE;
        }

        memset (&ra, 0, sizeof(struct sockaddr_in));
        ra.sin_family = AF_INET;
        ra.sin_addr.s_addr = handler->detected_servers[id].ip;
        ra.sin_port = handler->detected_servers[id].port;

        int err = connect (
            handler->socket_fd,
            (__CONST_SOCKADDR_ARG) &ra,
            sizeof(struct sockaddr_in));
        if (err)
        {
            client_disconnect (handler);
            return E_CONNECTION_FAILURE;
        }

        if (pthread_create (&handler->receive_thread,
        NULL, recive_thread, handler))
        {
            client_disconnect (handler);
            return E_CONNECTION_FAILURE;
        }
    }

    return 0;
}

Status client_disconnect(ClientHandler param)
{
    ClientHandler_t handler = (ClientHandler_t) param;

    if (handler->socket_fd != 0)
    {
        close (handler->socket_fd);
        handler->socket_fd = 0;
    }

    return 0;
}

Status client_send_message(ClientHandler param, void * buffer, ssize_t bufferSize)
{
    ClientHandler_t handler = (ClientHandler_t) param;

    ssize_t status = send (handler->socket_fd, buffer, bufferSize, 0);

    return status;
}
