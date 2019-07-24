#ifndef SERVER_H_
#define SERVER_H_

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_NAME_LEN 64

typedef enum
{
    E_OK,                 ///< Status OK
    E_CONNECTION_FAILURE, ///< Failure on connection
    E_NOT_MANAGED,        ///< Unmanaged client
    E_NOT_INITIALIZED,    ///< Not initialized
    E_FATAL_EXCEPTION     ///< Fatal exception
} Status;

typedef enum
{
    E_ALLOCATING_MEM,   ///< Error allocating memory
    E_OPENING_SOCKET,   ///< Error opening socket
    E_STARTING_THREAD,  ///< Error starting thread
    E_JOINING_THREAD,   ///< Error joining thread
    E_ADVERTISE_FAILURE ///< Error on advertise
} ServerErrorCode;

typedef void * ServerHandler;
typedef uint32_t ClientId;

/**
 * Callback prototype for reciving data.
 *
 * @param[in] handler    Reference to sever instance.
 * @param[in] clientId   Id of client from which data was recived
 * @param[in] buffer     Reference to received data
 * @param[in] size       Size of data recived
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
 * Callback prototype for server error
 *
 * @param[in] handler    Reference to sever instance.
 * @param[in] clientInfo Client details
 */
typedef void (*notify_cb_error)(ServerHandler handler, ServerErrorCode clientId);

typedef struct
{
    char * ip[16];           ///< Multicast address on which to advertise
    uint16_t advertise_port; ///< Advertising port
    uint16_t game_port;      ///< Game port
    uint16_t max_nb_clients; ///< Max accepted clients
    char name[MAX_NAME_LEN]; ///< Server name
    notify_cb_client client_connected_cb;    ///< Callback on new client
    notify_cb_client client_disconnected_cb; ///< Callback on disconnected client
    notify_cb_recive receive_cb;             ///< Callback on data received
    notify_cb_error  error_cb;               ///< Callback on error
} ServerConfig;

/**
 * Starts a new advertising server based on the provided config
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
 * @param[in] handler Refrence to sever instance.
 */
Status server_stop_advertising(ServerHandler handler);

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

#endif /* SERVER_H_*/
