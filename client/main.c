#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "client.h"

void recive_data_cb(ClientHandler handler, char *buffer, int size)
{
	printf("Recived %d bytes from server: %s\n", size, buffer);
}

int main(void)
{
	ServerConfig srvConfig;
	
	memcpy(srvConfig.ip, "224.0.0.26", sizeof(srvConfig.ip));
	memcpy(srvConfig.name, "Client1", sizeof(srvConfig.name));

	srvConfig.port = 6000;
	srvConfig.receiveCallback = recive_data_cb;

	ServerInfo servers[10];
	ClientHandler handler = client_init(&srvConfig);

	int count = client_list_servers(handler, servers, 10, 3);

	if (count > 0)
	{
		printf("Server found. Connecting to %s\n", servers[0].name);

		client_connect(handler, servers[0].ID);

		printf("Sending message to server\n");
		client_send_message(handler, "Ana are mere", 10);

		//wait 10 seconds and exit.
		sleep(10);

		printf("Disconecting\n");
		client_disconnect(handler);
	}
	else
	{
		printf("No servers found\n");
	}

	printf("Uninitializing client\n");

	return EXIT_SUCCESS;
}
