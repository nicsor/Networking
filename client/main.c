#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"

void recive_data_cb(ClientHandler handler, char *buffer, int size)
{
    printf ("Recived %d bytes from server: %s\n", size, buffer);
}

void error_cb(ClientHandler handler)
{
    perror ("Client error");
    exit (1);
}

int main(void)
{
    ClientConfig clientConfig;

    memcpy (clientConfig.ip, "224.0.0.26", sizeof(clientConfig.ip));

    clientConfig.max_nb_servers = 5;
    clientConfig.port = 6000;
    clientConfig.receive_cb = recive_data_cb;

    ServerDetails servers[10];
    ClientHandler handler = client_init (&clientConfig);

    int count = client_list_servers (handler, servers, 10, 500);

    if (count > 0)
    {
        printf ("Server found. Connecting to %s\n", servers[0].name);

        client_connect (handler, servers[0].id);

        printf ("Sending message to server\n");
        client_send_message (handler, "Ana are mere", 10);

        //wait 10 seconds and exit.
        sleep (5);

        printf ("Disconnecting\n");
        client_disconnect (handler);
    }
    else
    {
        printf ("No servers found\n");
    }

    return EXIT_SUCCESS;
}
