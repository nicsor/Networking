#include "client.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#ifdef ENABLE_DEBUG
#include <stdio.h>
#define DEBUG(...) fprintf (stderr, __VA_ARGS__)
#else
#define DEBUG(...)
#endif

/** Server information */
typedef struct
{
    in_addr_t ip;   ///< Address of server
    uint16_t port;  ///< Port on which the server is listening
} Server_t;

/* Client information */
typedef struct
{
    ClientConfig config;        ///< Client configuration
    int socket_fd;              ///< Connection file descriptor
    Server_t *detected_servers; ///< List of detected servers
    int detected_servers_count; ///< Detected servers count
} ClientInfo;

typedef ClientInfo * ClientHandler_t;

ClientHandler client_init(ClientConfig * config)
{
    ClientHandler_t handler = (ClientHandler_t) malloc (sizeof(ClientInfo));
    ssize_t sizeofServerData = sizeof(Server_t) * config->max_nb_servers;

    if (handler == NULL)
    {
        return NULL;
    }

    bzero (handler, sizeof(ClientInfo));
    handler->detected_servers = (Server_t*) malloc (sizeofServerData);

    if (handler->detected_servers == NULL)
    {
        free (handler);
        return NULL;
    }

    bzero (handler->detected_servers, sizeofServerData);
    handler->config = *config;

    return handler;
}

void client_deinit(ClientHandler handler)
{
    client_disconnect (handler);
    free (handler);
}

uint16_t client_list_servers(
    ClientHandler param,
    ServerDetails *servers,
    uint16_t maxServerCount,
    uint16_t timeoutMs)
{
    ClientHandler_t handler = (ClientHandler_t) param;

    struct sockaddr_in addr = {0};
    unsigned int addrlen = sizeof(addr);
    int sock, status;
    char message[1024] = {0};

    /* set up socket */
    sock = socket (AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        return 0;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr ((const char *) handler->config.ip);
    addr.sin_port = htons (handler->config.port);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeoutMs * 1000;
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

    status = sendto (sock, ADVERTISING_REQUEST, sizeof(ADVERTISING_REQUEST), 0, (struct sockaddr *) &addr, sizeof(addr));

    if (status == -1)
    {
        close (sock);
        return 0;
    }

    handler->detected_servers_count = 0;

    int nbMaxServers =
        (maxServerCount > handler->config.max_nb_servers) ?
            handler->config.max_nb_servers : maxServerCount;

    while (handler->detected_servers_count < nbMaxServers)
    {
        int bytesRcvd = recvfrom (
            sock,
            (void *) message,
            sizeof(message),
            (int) 0,
            (struct sockaddr *) &addr,
            (unsigned int *) &addrlen);

        if (bytesRcvd <= 0)
        {
            // Timeout or error, exit
            break;
        }
        else if ((bytesRcvd >= 6) &&
            (strncmp(message, ADVERTISING_RESPONSE, sizeof(ADVERTISING_RESPONSE)) == 0))
        {
            // Expected response received.
            int offset = sizeof(ADVERTISING_RESPONSE);
            Server_t * server = &handler->detected_servers[handler->detected_servers_count];
            ServerDetails * serverInfo = &servers[handler->detected_servers_count];
            DEBUG ("Received %d bytes", bytesRcvd);

            int nameLength = bytesRcvd - (offset + 2);
            int maxlen     = (nameLength > sizeof(serverInfo->name)) ?
                sizeof(serverInfo->name) : nameLength;

            serverInfo->id = handler->detected_servers_count;
            memcpy  (&server->port,    &message[offset],   2);
            strncpy (serverInfo->name, &message[offset+2], maxlen);

            server->ip   = addr.sin_addr.s_addr;
            server->port = ntohs (server->port);

            DEBUG ("Server detected: %s %d\n", serverInfo->name, server->port);

            ++handler->detected_servers_count;
        }
    }

    close (sock);

    return handler->detected_servers_count;
}

void *receive_thread(void *handler)
{
    char buffer[1024];
    ClientHandler_t instance = (ClientHandler_t) handler;

    pthread_detach(pthread_self());

    while (1)
    {
        int dataLength = recv (instance->socket_fd, buffer, sizeof(buffer), 0);

        if (dataLength <= 0)
        {
            client_disconnect (instance);
            break;
        }

        instance->config.receive_cb (instance, buffer, dataLength);
    }

    pthread_exit(NULL);
}

Status client_connect(ClientHandler handler, ServerId serverId)
{
    ClientHandler_t instance = (ClientHandler_t) handler;
    Status status = E_NOT_MANAGED;

    if (serverId < instance->detected_servers_count)
    {
        struct sockaddr_in sa = {0};

        instance->socket_fd = socket (PF_INET, SOCK_STREAM, 0);

        if (instance->socket_fd != -1)
        {
            pthread_t threadId;
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = instance->detected_servers[serverId].ip;
            sa.sin_port = instance->detected_servers[serverId].port;

            if (connect (instance->socket_fd, (const struct sockaddr *)&sa, (socklen_t)sizeof(sa)) ||
                (pthread_create (&threadId, NULL, receive_thread, instance)))
            {
                close (instance->socket_fd);
                instance->socket_fd = 0;

                status = E_NOT_INITIALIZED;
            }
            else
            {
                DEBUG("Client: State update[Connected to server]\n");

                status = E_OK;
            }
        }
        else
        {
            instance->socket_fd = 0;

            status = E_NOT_INITIALIZED;
        }
    }

    return status;
}

Status client_disconnect(ClientHandler handler)
{
    ClientHandler_t instance = (ClientHandler_t) handler;
    Status status = E_NOT_INITIALIZED;

    if (instance->socket_fd != 0)
    {
        DEBUG("Client: State update[Disconnecting from server]\n");

        shutdown (instance->socket_fd, SHUT_RDWR);
        instance->socket_fd = 0;

        if (instance->config.disconnect_cb != NULL)
        {
            instance->config.disconnect_cb(instance);
        }

        status = E_OK;
    }

    return status;
}

Status client_send_message(ClientHandler handler, void * buffer, ssize_t bufferSize)
{
    ClientHandler_t instance = (ClientHandler_t) handler;
    Status status = E_NOT_INITIALIZED;

    if (instance->socket_fd != 0)
    {
        DEBUG("Client: State update[Sending message to server]\n");

        ssize_t len = send (instance->socket_fd, buffer, bufferSize, 0);

        status = (len < 0) ? E_ERR_ON_SEND : E_OK;
    }

    return status;
}
