#ifndef NETWORKING_CLIENT_SERVER_CFG_H_
#define NETWORKING_CLIENT_SERVER_CFG_H_

#define MAX_NAME_LEN 64
#define ADVERTISING_REQUEST  "Marco"
#define ADVERTISING_RESPONSE "Polo"

typedef enum
{
    E_OK,                 ///< Status OK
    E_ERR_ON_SEND,        ///< Error on send
    E_NOT_MANAGED,        ///< Unmanaged server
    E_NOT_INITIALIZED     ///< Not initialized
} Status;

#endif /* NETWORKING_CLIENT_SERVER_CFG_H_*/
