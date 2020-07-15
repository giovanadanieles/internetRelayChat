/* Server - Major Steps:
	- Create a socket with socket();
	- Bind the socket to an address using bind();
	- Listen for connections with listen();
	- Accept a connection with accept();
	- Send and receive data, using read() and write() system calls. */

#include "string_manipulation.h"
#include "server_operation.h"

// /* Atomic objects are the only objects that are free from data races,
//  that is, they may be modified by two threads concurrently or
//  modified by one and read by another. */
// static _Atomic unsigned int cliCount = 0;
// static int userID = 0;

int main(int argc, char* const argv[]) {

	char* IP = "0.0.0.0";
	int port = 1234;

	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in server_addr, client_addr;
	pthread_t tid;

	initialize_channel_list();

	/* -------------------------- Socket settings --------------------------

	  AF_INET is an address family that designates IPv4 as the address' type
	 the socket can communicate with;
	  SOCK_STREAM defines the communication type - in this case, TCP;
	  The third argument defines the protocol value for IP. */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	// IP and port are binded and a connection will be opened based on both.
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(IP);
	server_addr.sin_port = htons(port);

	/* Pipe signals are software generated interrupts.
	  SIGPIPE is sent to a process when it attempts to write to a pipe
	 whose read end is closed; SIG_IGN sets SIGPIPE signal to be ignored. */
	signal(SIGPIPE, SIG_IGN);

	/* This helps manipulating options for the socket referred by the
	 descriptor sockfd; it also prevents errors. */
   if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*) &option, sizeof(option)) < 0) {
       printf("\nErro: setsockopt.\n");

       // EXIT FAILURE;
       exit(1);
   }

	/* After creating the socket, the bind() function binds the
	 socket to the address and the port number specified in addr. */
	if (bind(listenfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
		printf("\nErro: bind.\n");

		// EXIT FAILURE
		exit(1);
	}

	/* The listen() function puts the server socket in a passsive mode,
	 where it waits for a client's approach to make a connection. */
	if (listen(listenfd, 10) < 0){
		printf("\nErro: listen.\n");

		// EXIT FAILURE
		exit(1);
	}

	// --------------------------------------- The Chatroom ----------------------------------
	// If there has been no errors so far, the chat server will be available.

	printf("\033[1;32m");
	printf(" _______  _______  _______  _______         _______  _______  _______  _______ ");
	printf("\n|  _    ||   _   ||       ||       |       |       ||   _   ||       ||       |");
	printf("\n| |_|   ||  |_|  ||_     _||    ___| ____  |    _  ||  |_|  ||    _  ||   _   |");
	printf("\n|       ||       |  |   |  |   |___ |____| |   |_| ||       ||   |_| ||  | |  |");
	printf("\n|  _   | |       |  |   |  |    ___|       |    ___||       ||    ___||  |_|  |");
	printf("\n| |_|   ||   _   |  |   |  |   |___        |   |    |   _   ||   |    |       |");
	printf("\n|_______||__| |__|  |___|  |_______|       |___|    |__| |__||___|    |_______|");
	printf("\n  ___   _  _______  ___      ___   __    _  ___   _  __   __  _______  ___      ");
	printf("\n |   | | ||   _   ||   |    |   | |  |  | ||   | | ||  | |  ||       ||   |     ");
	printf("\n |   |_| ||  |_|  ||   |    |   | |   |_| ||   |_| ||  | |  ||   _   ||   |     ");
	printf("\n |      _||       ||   |    |   | |       ||      _||  |_|  ||  | |  ||   |     ");
	printf("\n |     |_ |       ||   |___ |   | |  _    ||     |_ |       ||  |_|  ||   |___  ");
	printf("\n |    _  ||   _   ||       ||   | | | |   ||    _  ||       ||       ||       | ");
	printf("\n |___| |_||__| |__||_______||___| |_|  |__||___| |_||_______||_______||_______| \n");
	printf("\n ______________________________________________________________________________ \n\n\n");
	printf("\033[0m");

	/*  "Infinite loop": responsible for communicating, receiving messages
	 from a client and sending them to everyone else. */
	while (1) {

		socklen_t cliLen = sizeof(client_addr);
		
		/* Responsible for extracting the first connection request on the
		 queue of pending connections, creating a new connected socket
		 and returning a new file descriptor referring to that socket. */
		connfd = accept(listenfd, (struct sockaddr*) &client_addr, &cliLen);

		is_server_full(connfd);

		// -------------------- Client Management --------------------
		/* Defines client settings, adds it to the queue, 
		creates a thread and a new function to handle client. */
		Client* cli = (Client*) malloc(sizeof(Client));
		create_client(client_addr, connfd, cli);

		// Starts a new thread in the calling process
		pthread_create(&tid, NULL, &handle_client, (void*) cli);

		// Reducing CPU usage
		sleep(1);
	}

	// EXIT SUCCESS
	return 0;
}
