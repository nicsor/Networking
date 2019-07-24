#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "server.h"

volatile int clientsCount  = 0;
ClientId availableClientId = 0;

void recive_data_cb(
		ServerHandler handler, 
		ClientId clientId,
		char *buffer,
		int size)
{
	printf("Recived %d bytes from %d: %s\n", size, clientId, buffer);

	server_send_message(handler, "Salut", sizeof("Salut"));
}

void client_connected_cb(ClientInfo clientInfo)
{
	printf("Client %d connected: %s\n", clientInfo.ID, clientInfo.name);
	++clientsCount;
	availableClientId = clientInfo.ID;
}

void client_disconnected_cb(ClientInfo clientInfo)
{
	printf("Client %d disconnected: %s\n", clientInfo.ID, clientInfo.name);
	--clientsCount;
}

int main(void) {

	ServerConfig srvConfig;

	memcpy(srvConfig.ip, "224.0.0.26", sizeof(srvConfig.ip));
	memcpy(srvConfig.name, "SuperServer", sizeof(srvConfig.name));

	srvConfig.advertisePort = 6000;
	srvConfig.gamePort      = 6001;
	srvConfig.clientConnectedCallback = client_connected_cb;
	srvConfig.clientDisconnectedCallback = client_disconnected_cb;
	srvConfig.receiveCallback = recive_data_cb;

	ServerHandler handler = server_init(&srvConfig);

	while (clientsCount == 0)
	{
		// Wait for at least one client to appear.
		sleep(1);
	}

	server_stop_advertising(handler);

	server_send_message(handler, "Salut", sizeof("Salut"));

	//wait 10 seconds and exit.
	sleep(10);

	server_deinit(handler);

	return 0;
}
