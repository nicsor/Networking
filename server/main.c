#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "server.h"

volatile int clientsCount = 0;
ClientId availableClientId = 0;

void recive_data_cb(ServerHandler handler, ClientId clientId, void *buffer, ssize_t size)
{
    printf ("Recived %d bytes from %d: %s\n", (int) size, clientId, (char *) buffer);

    server_send_message (handler, "Salut", sizeof("Salut"));
}

void client_connected_cb(ServerHandler handler, ClientId clientId)
{
    printf ("Client %d connected\n", clientId);
    ++clientsCount;
    availableClientId = clientId;
}

void client_disconnected_cb(ServerHandler handler, ClientId clientId)
{
    printf ("Client %d disconnected\n", clientId);
    --clientsCount;
}

void error_cb(ServerHandler handler, ServerErrorCode errorCB)
{
    perror ("Server error");
    exit (1);
}

int main(void)
{
    ServerConfig srvConfig;

    memcpy (srvConfig.ip, "224.0.0.26", sizeof(srvConfig.ip));
    memcpy (srvConfig.name, "SuperServer", sizeof(srvConfig.name));

    srvConfig.advertise_port = 6000;
    srvConfig.game_port = 6001;
    srvConfig.client_connected_cb = client_connected_cb;
    srvConfig.client_disconnected_cb = client_disconnected_cb;
    srvConfig.receive_cb = recive_data_cb;
    srvConfig.error_cb = error_cb;
    srvConfig.max_nb_clients = 10;

    ServerHandler handler = server_init (&srvConfig);

    while (clientsCount == 0)
    {
        // Wait for at least one client to appear.
        sleep (1);
    }

    server_stop_advertising (handler);

    server_send_message (handler, "Salut", sizeof("Salut"));

    //wait 10 seconds and exit.
    sleep (10);

    server_deinit (handler);

    return 0;
}
