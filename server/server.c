#include "server.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

typedef struct
{
    ClientId id;
    struct ServerInfo * handler;
    pthread_t client_thread;
    int socket_fd;
} ClientData;

typedef struct ServerInfo
{
    ServerConfig config;
    pthread_t advertise_thread;
    pthread_t game_thread;
    int advertise_fd;
    int game_fd;
    int advertise;
    int initialized;
    ClientData *clientData;
} ServerInfo;

typedef ServerInfo * ServerHandler_t;

void server_fatal_error(ServerHandler_t handler, ServerErrorCode errorCode)
{
    if (handler->config.error_cb != NULL)
    {
        handler->config.error_cb (handler, errorCode);
    }

    server_deinit (handler);
}

void *advertise_thread(void *param)
{
    ServerHandler_t instance = (ServerHandler_t) param;
    uint16_t port = htons (instance->config.game_port);
    char message[MAX_NAME_LEN + sizeof(port)];
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    memcpy (message, instance->config.name, 20);
    memcpy (&message[20], &port, sizeof(port));

    printf ("State update: start advertising\n");

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt (instance->advertise_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

    while (instance->advertise)
    {
        char incoming[sizeof(message)];
        int bytesTransfered = recvfrom (
                                        instance->advertise_fd,
                                            incoming,
                                            sizeof(incoming),
                                            0,
                                            (struct sockaddr *) &addr,
                                            &addrlen);

        if (bytesTransfered < 0)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                // timeout
                continue;
            }
            else
            {
                server_fatal_error (instance, E_CONNECTION_FAILURE);
                return NULL;
            }
        }
        else if (bytesTransfered == 0)
        {
            break;
        }

        bytesTransfered = sendto (
                                  instance->advertise_fd,
                                      message,
                                      sizeof(message),
                                      0,
                                      (struct sockaddr *) &addr,
                                      addrlen);

        if (bytesTransfered < 0)
        {
            server_fatal_error (instance, E_ADVERTISE_FAILURE);
            return NULL;
        }
        else if (bytesTransfered == 0)
        {
            break;
        }
    }

    printf ("State update: stop advertising\n");

    return NULL;
}

void *client_thread(void *param)
{
    ClientData * clientData = (ClientData *) param;
    char message[1024];

    while (1)
    {
        int bytesRcvd = recv (clientData->socket_fd, message, sizeof(message), 0);

        if (bytesRcvd < 0)
        {
            // some error on client. assume disconnected
            break;
        }
        else if (bytesRcvd == 0)
        {
            break;
        }

        if (clientData->handler->config.receive_cb != NULL)
        {
            clientData->handler->config.receive_cb (
                                                    clientData->handler,
                                                        clientData->id,
                                                        message,
                                                        bytesRcvd);
        }
    }

    if (clientData->handler->config.client_disconnected_cb != NULL)
    {
        clientData->handler->config.client_disconnected_cb (clientData->handler, clientData->id);
    }

    close (clientData->socket_fd);

    return NULL;
}

void *game_thread(void *param)
{
    ServerHandler_t instance = (ServerHandler_t) param;
    struct sockaddr_in sa;

    instance->game_fd = socket (PF_INET, SOCK_STREAM, 0);

    if (instance->game_fd < 0)
    {
        instance->game_fd = 0;
        server_fatal_error (instance, E_CONNECTION_FAILURE);
        return NULL;
    }

    memset (&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = instance->config.game_port;

    if (bind (instance->game_fd, (struct sockaddr *) &sa, sizeof(sa)) == -1)
    {
        server_fatal_error (instance, E_CONNECTION_FAILURE);
        return NULL;
    }

    listen (instance->game_fd, 5);

    printf ("State update: start listening for clients\n");

    while (1)
    {
        struct sockaddr_in isa;
        socklen_t addr_size = sizeof(isa);
        int clientId = 0;

        // Check for free client slot
        for (clientId = 0; clientId < instance->config.max_nb_clients; ++clientId)
        {
            if (instance->clientData[clientId].socket_fd == 0)
            {
                break;
            }
        }

        if (clientId == instance->config.max_nb_clients)
        {
            // server is full
            break;
        }

        instance->clientData[clientId].socket_fd = accept (
                                                           instance->game_fd,
                                                               (struct sockaddr*) &isa,
                                                               &addr_size);
        instance->clientData[clientId].handler = instance;
        instance->clientData[clientId].id = clientId;

        printf ("State update: client connected\n");

        if (instance->clientData[clientId].socket_fd < 0)
        {
            instance->clientData[clientId].socket_fd = 0;
            server_fatal_error (instance, E_CONNECTION_FAILURE);
            return NULL;
        }

        if (instance->advertise == 0)
        {
            // Darn ... should have stopped
            close (instance->game_fd);
            instance->clientData[clientId].socket_fd = 0;
            break;
        }

        if (pthread_create (
            &instance->clientData[clientId].client_thread,
            NULL,
            client_thread,
            &instance->clientData[clientId]))
        {
            server_fatal_error (instance, E_STARTING_THREAD);
            return NULL;
        }

        if (instance->config.client_connected_cb != NULL)
        {
            instance->config.client_connected_cb (instance, clientId);
        }
    }

    printf ("State update: stop listening for clients\n");

    close (instance->game_fd);

    return NULL;
}

ServerHandler server_init(ServerConfig *config)
{
    ServerHandler_t handler = (ServerHandler_t) malloc (sizeof(ServerInfo));
    memset (handler, 0, sizeof(ServerInfo));

    if (handler == NULL)
    {
        if ((config != NULL) && (config->error_cb != NULL))
        {
            config->error_cb (NULL, E_ALLOCATING_MEM);
        }
        return NULL;
    }

    handler->config = *config;
    handler->advertise = 1;

    ssize_t sizeofClientData = sizeof(ClientData) * handler->config.max_nb_clients;
    handler->clientData = (ClientData*) malloc (sizeofClientData);

    if (handler->clientData == NULL)
    {
        server_fatal_error (handler, E_ALLOCATING_MEM);
        return NULL;
    }

    memset (handler->clientData, 0, sizeofClientData);

    handler->advertise_fd = socket (AF_INET, SOCK_DGRAM, 0);
    if (handler->advertise_fd < 0)
    {
        handler->advertise_fd = 0;
        server_fatal_error (handler, E_CONNECTION_FAILURE);
        return NULL;
    }

    struct sockaddr_in addr;
    bzero ((char *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl (INADDR_ANY);
    addr.sin_port = htons (config->advertise_port);

    if (bind (handler->advertise_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        server_fatal_error (handler, E_CONNECTION_FAILURE);
        return NULL;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr ((const char *) config->ip);
    mreq.imr_interface.s_addr = htonl (INADDR_ANY);

    if (setsockopt (handler->advertise_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        server_fatal_error (handler, E_STARTING_THREAD);
        return NULL;
    }

    if (pthread_create (&handler->game_thread, NULL, game_thread, handler))
    {
        server_fatal_error (handler, E_STARTING_THREAD);
        return NULL;
    }

    if (pthread_create (&handler->advertise_thread, NULL, advertise_thread, handler))
    {
        server_fatal_error (handler, E_STARTING_THREAD);
        return NULL;
    }

    handler->initialized = 1;

    return handler;
}

void server_deinit(ServerHandler param)
{
    ServerHandler_t handler = (ServerHandler_t) param;

    handler->initialized = 0;

    server_stop_advertising (handler);

    // Stop all client threads
    for (ClientId clientId = 0; clientId < handler->config.max_nb_clients; ++clientId)
    {
        if (handler->clientData[clientId].socket_fd != 0)
        {
            close (handler->clientData[clientId].socket_fd);
            handler->clientData[clientId].socket_fd = 0;

            if (pthread_join (handler->clientData[clientId].client_thread, NULL))
            {
                server_fatal_error (handler, E_JOINING_THREAD);
                return;
            }
        }
    }

    free (handler);
}

Status server_stop_advertising(ServerHandler param)
{
    ServerHandler_t handler = (ServerHandler_t) param;

    handler->advertise = 0;

    if (handler->advertise_fd != 0)
    {
        printf ("State update: Stopping advertising\n");

        close (handler->advertise_fd);
        handler->advertise_fd = 0;

        if (pthread_join (handler->advertise_thread, NULL))
        {
            server_fatal_error (handler, E_JOINING_THREAD);
            return E_FATAL_EXCEPTION;
        }

        printf ("State update: Advertising stopped\n");
    }

    return E_OK;
}

Status server_send_message(ServerHandler param, void * buffer, ssize_t bufferSize)
{
    ServerHandler_t handler = (ServerHandler_t) param;

    if (!handler->initialized)
    {
        return E_NOT_INITIALIZED;
    }

    for (ClientId clientId = 0; clientId < handler->config.max_nb_clients; ++clientId)
    {
        server_send_message_to_client (handler, clientId, buffer, bufferSize);
    }

    return E_OK;
}

Status server_send_message_to_client(
    ServerHandler param,
    ClientId clientId,
    void * buffer,
    ssize_t bufferSize)
{
    ServerHandler_t handler = (ServerHandler_t) param;
    Status status = E_NOT_MANAGED;

    if (!handler->initialized)
    {
        return E_NOT_INITIALIZED;
    }

    if ((clientId < handler->config.max_nb_clients)
        && (handler->clientData[clientId].socket_fd != 0))
    {
        printf ("Sending message to client %d\n", clientId);

        send (handler->clientData[clientId].socket_fd, buffer, bufferSize, 0);

        status = E_OK;
    }

    return status;
}
