#ifndef NETWORKING_CLIENT_H_
#define NETWORKING_CLIENT_H_

#include "client_server_cfg.h"

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef uint16_t ServerId;
    typedef void * ClientHandler;
    typedef void (*client_notify_cb_receive)(ClientHandler handler, char *buffer, int size);

    /**
     * Callback prototype for client error
     *
     * @param[in] handler  Reference to client instance.
     */
    typedef void (*client_notify_cb_disconnect)(ClientHandler handler);

    /** Client configuration */
    typedef struct
    {
        char * ip[16];                             ///< Multicast address where servers advertise
        uint16_t port;                             ///< Port on which to check for servers
        uint16_t max_nb_servers;                   ///< Maximum number of servers to list
        client_notify_cb_receive receive_cb;       ///< Handler for callback on new data
        client_notify_cb_disconnect disconnect_cb; ///< Handler for callback on disconnect
    } ClientConfig;

    /** Server details */
    typedef struct
    {
        ServerId id;             ///< Server id
        char name[MAX_NAME_LEN]; ///< Server name
    } ServerDetails;

    /**
     * Starts a new client based on the provided configuration.
     *
     * @param[in] config Reference to client configuration
     */
    ClientHandler client_init(ClientConfig * config);

    /**
     * Stops client and release any resources.
     *
     * @param[in] handler Reference to client instance.
     */
    void client_deinit(ClientHandler handler);

    /**
     * Connect to a specific server instance.
     *
     * @param[in] handler  Reference to client instance.
     * @param[in] serverId Id of server to connect to.
     *     The available ids are retrieved upon calling client_list_servers.
     */
    Status client_connect(ClientHandler handler, ServerId serverId);

    /**
     * Close existing connection to server
     *
     * @param[in] handler  Reference to client instance.
     */
    Status client_disconnect(ClientHandler handler);

    /**
     * Check servers that advertise on the configured instance.
     *
     * @param[in]  handler        Reference to client instance.
     * @param[out] servers        Detected servers.
     * @param[in]  maxServerCount Maximum number of servers to be retrieved.
     * @param[in]  timeoutMs      Timeout in milliseconds before returning if
     *     max number of servers is not found.
     */
    uint16_t client_list_servers(
        ClientHandler handler,
        ServerDetails *servers,
        uint16_t maxServerCount,
        uint16_t timeoutMs);

    /**
     * Send message to server
     *
     * @param[in] handler    Reference to sever instance
     * @param[in] buffer     Reference to data to be sent
     * @param[in] bufferSize Size of data to be sent
     */
    Status client_send_message(ClientHandler handler, void * buffer, ssize_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif /* NETWORKING_CLIENT_H_*/
