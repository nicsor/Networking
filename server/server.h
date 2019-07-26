#ifndef NETWORKING_SERVER_H_
#define NETWORKING_SERVER_H_

#include "client_server_cfg.h"

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void * ServerHandler;
    typedef uint32_t ClientId;

    /**
     * Callback prototype for receiving data.
     *
     * @param[in] handler    Reference to sever instance.
     * @param[in] clientId   Id of client from which data was received
     * @param[in] buffer     Reference to received data
     * @param[in] size       Size of data received
     */
    typedef void (*notify_cb_recive)(
        ServerHandler handler,
        ClientId clientId,
        void *buffer,
        ssize_t size);

    /**
     * Callback prototype for client state update(connected/disconnected)
     *
     * @param[in] handler    Reference to sever instance.
     * @param[in] clientInfo Client details
     */
    typedef void (*notify_cb_client)(ServerHandler handler, ClientId clientId);

    /**
     * Callback prototype for server error.
     * The provided handler is offered as information on the failed instance.
     * No more actions should be performed on it. Resources are automatically
     * freed upon callback exit.
     *
     * @param[in] handler    Reference to sever instance.
     */
    typedef void (*notify_cb_error)(ServerHandler handler);

    typedef struct
    {
            char * ip[16];           ///< Multicast address on which to advertise
            uint16_t advertise_port; ///< Advertising port
            uint16_t game_port;      ///< Server listening port
            uint16_t max_nb_clients; ///< Max accepted clients
            char name[MAX_NAME_LEN]; ///< Server name
            notify_cb_client client_connected_cb;    ///< Handler to callback on new client
            notify_cb_client client_disconnected_cb; ///< Handler to callback on client disconnect
            notify_cb_recive receive_cb;             ///< Handler to callback on data received
            notify_cb_error error_cb;                ///< Handler to callback on error
    } ServerConfig;

    /**
     * Starts a new advertising server based on the provided configuration.
     *
     * @param[in] config Reference to server configuration
     */
    ServerHandler server_init(ServerConfig * config);

    /**
     * Stops server and closes all client sockets
     *
     * @param[in] handler Reference to sever instance.
     */
    void server_deinit(ServerHandler handler);

    /**
     * Stops advertising presence and stops accepting new clients
     *
     * @param[in] handler Reference to sever instance.
     */
    Status server_stop_advertising(ServerHandler handler);

    /**
     * Remove client
     *
     * @param[in] handler  Reference to server instance.
     * @param[in] clientId Id of the client to be removed
     */
    Status server_remove_client(ServerHandler handler, ClientId clientId);

    /**
     * Send message to all connected clients
     *
     * @param[in] handler    Reference to sever instance.
     * @param[in] buffer     Reference to data to be sent
     * @param[in] bufferSize Data size
     */
    Status server_send_message(ServerHandler handler, void * buffer, ssize_t bufferSize);

    /**
     * Send message to specific client
     *
     * @param[in] handler    Reference to sever instance
     * @param[in] clientId   Id of the client to which data is to be sent
     * @param[in] buffer     Reference to data to be sent
     * @param[in] bufferSize Size of data to be sent
     */
    Status server_send_message_to_client(
        ServerHandler handler,
        ClientId clientId,
        void * buffer,
        ssize_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif /* NETWORKING_SERVER_H_*/
