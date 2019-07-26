#include "server.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef ENABLE_DEBUG
#include <stdio.h>
#define DEBUG(...) fprintf (stderr, __VA_ARGS__)
#else
#define DEBUG(...)
#endif

/** Client details */
typedef struct
{
    ClientId id;                 ///< Client ID
    struct ServerInfo * handler; ///< Server handler
    int socket_fd;               ///< Client assigned socket
} ClientData;

/** Server details */
typedef struct ServerInfo
{
    ServerConfig config;        ///< Server configuration
    pthread_t advertise_thread; ///< Advertise thread handler
    pthread_t game_thread;      ///< Game handler
    int advertise_fd;           ///< Server advertise socket
    int game_fd;                ///< Server conn socket
    int is_advertising;         ///< Advertising state
    int is_initialized;         ///< Initialized state
    ClientData *client_data;    ///< Client data
} ServerInfo;

typedef ServerInfo * ServerHandler_t;

static void server_fatal_error(ServerHandler_t instance)
{
    if (instance->config.error_cb != NULL)
    {
        instance->config.error_cb (instance);
    }

    server_deinit (instance);
}

void *advertise_thread(void *param)
{
    ServerHandler_t instance = (ServerHandler_t) param;
    uint16_t port = htons (instance->config.game_port);
    const int offset = sizeof(ADVERTISING_RESPONSE);
    char message[MAX_NAME_LEN + sizeof(port) + sizeof(ADVERTISING_RESPONSE)] = {0};
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    // Prepare server advertise message
    memcpy (&message[0], ADVERTISING_RESPONSE, offset);
    memcpy (&message[offset],     &port,       2);
    memcpy (&message[offset + 2], instance->config.name, strlen(instance->config.name));

    while (1)
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
                // Timeout
                continue;
            }
            else if (instance->is_advertising)
            {
                server_fatal_error (instance);
                return NULL;
            }
            else
            {
                // Normal exit on stop advertising
                break;
            }
        }
        else if ((bytesTransfered == sizeof(ADVERTISING_REQUEST)) &&
            (strcmp(incoming, ADVERTISING_REQUEST) == 0))
        {
            sendto (
                instance->advertise_fd,
                message,
                sizeof(message),
                0,
                (struct sockaddr *) &addr,
                addrlen);
        }
    }

    return NULL;
}

void *client_thread(void *param)
{
    ClientData * clientData = (ClientData *) param;
    char message[1024];

    pthread_detach(pthread_self());

    while (1)
    {
        int bytesRcvd = recv (clientData->socket_fd, message, sizeof(message), 0);

        if (bytesRcvd <= 0)
        {
            close (clientData->socket_fd);
            clientData->socket_fd = 0;

            // Some error on client or disconnected. Exit
            if (clientData->handler->config.client_disconnected_cb != NULL)
            {
                clientData->handler->config.client_disconnected_cb (clientData->handler, clientData->id);
            }

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

    pthread_exit(NULL);
}

void *game_thread(void *param)
{
    ServerHandler_t instance = (ServerHandler_t) param;
    struct sockaddr_in sa = {0};

    instance->game_fd = socket (PF_INET, SOCK_STREAM, 0);

    if (instance->game_fd == -1)
    {
        instance->game_fd = 0;
        server_fatal_error (instance);
        return NULL;
    }

    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = instance->config.game_port;

    if (bind (instance->game_fd, (struct sockaddr *) &sa, sizeof(sa)) == -1)
    {
        server_fatal_error (instance);
        return NULL;
    }

    listen (instance->game_fd, instance->config.max_nb_clients);

    DEBUG ("Server: State update[Start listening for clients]\n");

    while (1)
    {
        struct sockaddr_in isa;
        pthread_t          threadId;
        socklen_t          addr_size = sizeof(isa);
        int                clientId  = 0;

        // Check for free client slot
        for (clientId = 0; clientId < instance->config.max_nb_clients; ++clientId)
        {
            if (instance->client_data[clientId].socket_fd == 0)
            {
                break;
            }
        }

        if (clientId == instance->config.max_nb_clients)
        {
            // Server is full, stop advertising
            server_stop_advertising(instance);
            break;
        }

        int clientFd = accept (
            instance->game_fd,
            (struct sockaddr*) &isa,
            &addr_size);

        if (clientFd == -1)
        {
            // Stop accepting client
            server_stop_advertising(instance);
            break;
        }

        DEBUG ("Server: State update[Client connected]\n");

        instance->client_data[clientId].id        = clientId;
        instance->client_data[clientId].handler   = instance;
        instance->client_data[clientId].socket_fd = clientFd;

        if (pthread_create (&threadId, NULL, client_thread, &instance->client_data[clientId]))
        {
            server_fatal_error (instance);
            return NULL;
        }

        if (instance->config.client_connected_cb != NULL)
        {
            instance->config.client_connected_cb (instance, clientId);
        }
    }

    DEBUG ("Server: State update[Stop listening for clients]\n");

    return NULL;
}

ServerHandler server_init(ServerConfig *config)
{
    ssize_t sizeofClientData = sizeof(ClientData) * config->max_nb_clients;
    ServerHandler_t handler = (ServerHandler_t) malloc (sizeof(ServerInfo));

    if (handler == NULL)
    {
        return NULL;
    }

    bzero (handler, sizeof(ServerInfo));
    handler->client_data = (ClientData*) malloc (sizeofClientData);

    if (handler->client_data == NULL)
    {
        free (handler);
        return NULL;
    }

    bzero (handler->client_data, sizeofClientData);

    handler->advertise_fd = socket (AF_INET, SOCK_DGRAM, 0);
    if (handler->advertise_fd == -1)
    {
        handler->advertise_fd = 0;
        server_fatal_error (handler);
        return NULL;
    }

    handler->config = *config;
    handler->is_advertising = 1;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl (INADDR_ANY);
    addr.sin_port = htons (config->advertise_port);

    if (bind (handler->advertise_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        server_fatal_error (handler);
        return NULL;
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr ((const char *) config->ip);
    mreq.imr_interface.s_addr = htonl (INADDR_ANY);

    if ((setsockopt (handler->advertise_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) ||
        (pthread_create (&handler->game_thread, NULL, game_thread, handler)) ||
        (pthread_create (&handler->advertise_thread, NULL, advertise_thread, handler)))
    {
        server_fatal_error (handler);
        return NULL;
    }

    DEBUG ("Server: State update[Initialized]\n");

    handler->is_initialized = 1;

    return handler;
}

void server_deinit(ServerHandler handler)
{
    ServerHandler_t instance = (ServerHandler_t) handler;

    instance->is_initialized = 0;

    server_stop_advertising (instance);

    // Stop all client threads
    for (ClientId clientId = 0; clientId < instance->config.max_nb_clients; ++clientId)
    {
        if (instance->client_data[clientId].socket_fd != 0)
        {
            close (instance->client_data[clientId].socket_fd);
            instance->client_data[clientId].socket_fd = 0;

            // Client threads are detached. => No join
        }
    }

    free (instance);
}

Status server_stop_advertising(ServerHandler handler)
{
    ServerHandler_t instance = (ServerHandler_t) handler;

    int advertisingFd = instance->advertise_fd;
    int gameFd        = instance->game_fd;

    instance->is_advertising = 0;

    if (advertisingFd != 0)
    {
        DEBUG("Server: State update[Stop advertising]\n");

        instance->advertise_fd = 0;
        shutdown (advertisingFd, SHUT_RDWR);

        pthread_join (instance->advertise_thread, NULL);

        DEBUG("Server: State update[Advertising stopped]\n");
    }

    if (gameFd != 0)
    {
        DEBUG("Server: State update[Stop listening]\n");

        instance->game_fd = 0;
        shutdown (gameFd, SHUT_RDWR);

        pthread_join (instance->game_thread, NULL);

        DEBUG("Server: State update[Listening stopped]\n");
    }

    return E_OK;
}

Status server_remove_client(ServerHandler handler, ClientId clientId)
{
    ServerHandler_t instance = (ServerHandler_t) handler;
    Status status = E_NOT_INITIALIZED;

    if (instance->is_initialized != 0)
    {
        const int isValidClientId = clientId < instance->config.max_nb_clients;
        const int isAvailableClient = isValidClientId &&
            (instance->client_data[clientId].socket_fd != 0);

        if (isAvailableClient)
        {
            DEBUG("Server: State update[Removing client %d]\n", clientId);

            close (instance->client_data[clientId].socket_fd);
            instance->client_data[clientId].socket_fd = 0;

            // Client threads are detached. => No join
        }
        else
        {
            status = E_NOT_MANAGED;
        }
    }

    return status;
}

Status server_send_message(ServerHandler handler, void * buffer, ssize_t bufferSize)
{
    ServerHandler_t instance = (ServerHandler_t) handler;
    Status status = E_NOT_INITIALIZED;

    if (instance->is_initialized != 0)
    {
        DEBUG("Server: State update[Sending broadcast message]\n");

        for (ClientId clientId = 0; clientId < instance->config.max_nb_clients; ++clientId)
        {
            server_send_message_to_client (instance, clientId, buffer, bufferSize);
        }

        status = E_OK;
    }

    return status;
}

Status server_send_message_to_client(
    ServerHandler handler,
    ClientId clientId,
    void * buffer,
    ssize_t bufferSize)
{
    ServerHandler_t instance = (ServerHandler_t) handler;
    Status status = E_NOT_INITIALIZED;

    if (instance->is_initialized != 0)
    {
        const int isValidClientId = clientId < instance->config.max_nb_clients;
        const int isAvailableClient = isValidClientId &&
            (instance->client_data[clientId].socket_fd != 0);

        if (isAvailableClient)
        {
            DEBUG("Server: State update[Sending message to client %d]\n", clientId);

            ssize_t len = send (instance->client_data[clientId].socket_fd, buffer, bufferSize, 0);

            status = (len < 0) ? E_ERR_ON_SEND : E_OK;
        }
        else
        {
            status = E_NOT_MANAGED;
        }
    }

    return status;
}
