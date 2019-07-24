#ifndef CLIENT_H_
#define CLIENT_H_

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
        E_OK, E_CONNECTION_FAILURE, E_NOT_MANAGED
    } Status;

    typedef enum
    {
        E_ALLOCATING_MEM,   ///< Error allocating memory
        E_OPENING_SOCKET,   ///< Error opening socket
        E_STARTING_THREAD,  ///< Error starting thread
        E_JOINING_THREAD,   ///< Error joining thread
        E_ADVERTISE_FAILURE ///< Error on advertise
    } ClientErrorCode;

    typedef uint16_t ServerId;
    typedef void * ClientHandler;
    typedef void (*notify_cb)(ClientHandler handler, char *buffer, int size);

    /**
     * Callback prototype for client error
     *
     * @param[in] handler  Reference to client instance.
     * @param[in] error    Error code
     */
    typedef void (*notify_cb_error)(ClientHandler handler, ClientErrorCode error);

    typedef struct
    {
            char * ip[16];
            uint16_t port;
            uint16_t max_nb_servers;
            notify_cb receiveCallback;
            notify_cb_error error_cb; ///< Callback on error
    } ClientConfig;

    typedef struct
    {
            ServerId ID;
            char name[MAX_NAME_LEN];
    } ServerInfo;

    ClientHandler client_init(ClientConfig * config);
    void client_deinit(ClientHandler handler);
    Status client_connect(ClientHandler handler, ServerId id);
    Status client_disconnect(ClientHandler handler);
    uint16_t client_list_servers(
        ClientHandler handler,
        ServerInfo *servers,
        uint16_t maxServerCount,
        uint16_t timeoutSec);
    Status client_send_message(ClientHandler handler, void * buffer, ssize_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif /* CLIENT_H_*/
