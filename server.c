/*
	Server - Major Steps:
	- Create a socket with socket();
	- Bind the socket to an address using bind();
	- Listen for connections with listen();
	- Accept a connection with accept();
	- Send and receive data, using read() and write() system calls.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLI 10
#define BUFFER_SZ 4097
#define NICK_LEN 16

// 	Atomic objects are the only objects that are free from data races, 
// that is, they may be modified by two threads concurrently or 
// modified by one and read by another.
static _Atomic unsigned int cliCount = 0; 
static int userID = 10;
//  leaveFlag indicates whether the client is connected of not or if there's an 
// error, that is, indicates if the client should be disconnected or not.
static int leaveFlag = 0;	
						
//  Client structure: stores the address, its socket descriptor, 
// the user ID and the nickname; makes client differentiation possible. 
typedef struct {
	struct sockaddr_in address;
	int sockfd;
	int userID;
	char nick[NICK_LEN];
} Client;

Client* clients[MAX_CLI];

// Necessary to send messages between the clients
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Responsible for overwriting and flushing the stdout
void str_overwrite_stdout() {
	printf("\r%s", "> ");
	fflush(stdout);
}

// Responsible for removing any undesirable '\n'
void str_trim(char* arr, int len) {
	for (int i = 0; i < len; i++) {
		if (arr[i] == '\n') {
			arr[i] = '\0';
			break;
		}
	}
}

// Adding clients in the array of clients
void add_client(Client* cli) {
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLI; i++) {
		if (!clients[i]) {
			clients[i] = cli;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Removing clients from the array of clients
void remove_client(int userID) {
	pthread_mutex_lock(&clients_mutex);

	// for (int i = 0; i < cliCount; i++) {
	for (int i = 0; i < MAX_CLI; i++) {
		if (clients[i]) {
			if (clients[i]->userID == userID) {
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Sending messages to all the clients, except the sernder itself
void send_message(char* msg, int userID) {
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLI; i++) {
		if (clients[i]) {
			if (clients[i]->userID != userID) {
				// If the write syscall fails:
				if (write(clients[i]->sockfd, msg, strlen(msg)) < 0 && leaveFlag == 0) {
					printf("Error: write to descriptor.\n");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Handles clients, assigns their values and joins the chat
void* handle_client(void* arg) {
	leaveFlag = 0;
	char buffer[BUFFER_SZ];
	char nick[NICK_LEN];
	
	cliCount++;

	Client* cli = (Client*) arg;

	//  Naming the client: 
	// recv() is used to receive messages from a socket;
	// Nicknames must be at least 3 characters long 
	// and should not exceed the maximum length established above.
	if (recv(cli->sockfd, nick, NICK_LEN, 0) <= 0 || strlen(nick) < 2 || strlen(nick) >= NICK_LEN - 1) {
		printf("Error: invalid nickname.\n");
		leaveFlag = 1;
	} else {
		strcpy(cli->nick, nick);
		//  Notifying other clients that this client has joined the chatroom
		// sprintf() is used to store formatted data as a string
		sprintf(buffer, "%s has joined!\n", cli->nick);
		printf("%s", buffer);

		send_message(buffer, cli->userID);
	}

	memset(buffer, '\0', BUFFER_SZ);

	// Exchange messages
	while (1) {
		if (leaveFlag) break;
		
		int receive = recv(cli->sockfd, buffer, BUFFER_SZ, 0);

		if (receive > 0) {
			if (strlen(buffer) > 0) {
				send_message(buffer, cli->userID);
				str_trim(buffer, strlen(buffer));
				// Prints on the server who sent the message to whom
				printf("%s\n", buffer);	
			}
		// Checks if the client wants to leave the chatroom		
		} else if (receive == 0 || strcmp(buffer, "exit") == 0) {
			sprintf(buffer, "%s has left.\n", cli->nick);
			printf("%s", buffer);

			send_message(buffer, cli->userID);

			leaveFlag = 1;
		} else {
			printf("Error.\n");
			leaveFlag = 1;
		}

		memset(buffer, '\0', BUFFER_SZ);
	}

	// The client has left the chat, so...
	close(cli->sockfd);
	remove_client(cli->userID);
	cliCount--;
	free(cli);
	// Marks the thread identified by thread as detached
	pthread_detach(pthread_self());

	return NULL;
}


int main(int argc, char* const argv[]) {
	if(argc != 2) {
		printf("Error. Try: %s <port>\n", argv[0]);

		// EXIT FAILURE
		return 1;
	}

	char* IP = "127.0.0.1";
	int port = atoi(argv[1]);

	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in server_addr, client_addr;
	pthread_t tid;

	// -------------------- Socket settings --------------------

	//  AF_INET is an address family that designates IPv4 as the address' type 
	// that the socket can communicate;
	//  SOCK_STREAM defines the communication type - in this case, TCP;
	//  The third argument defines the protocol value for IP.
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	// IP and port are binded and a connection will be opened based on both.
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(IP);
	server_addr.sin_port = htons(port);

	//  Pipe signals - software generated interrupts
	//  SIGPIPE is sent to a process when it attempts to write to a pipe 
	// whose read end is closed;
	//  SIG_IGN sets SIGPIPE signal to be ignored.
	signal(SIGPIPE, SIG_IGN); 

	// This helps in manipulating options for the socket referred by the 
	// descriptor sockfd; also prevents errors.
	if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*) &option, sizeof(option)) < 0) {
		printf("Error: setsockopt.\n");

		// EXIT FAILURE;
		exit(1);
	}

	// After creating the socket, the bind() function binds the socket to the 
	// address and the port number specified in addr.
	if (bind(listenfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
		printf("Error: bind.\n");

		// EXIT FAILURE
		exit(1);
	}

	// listen() puts the server socket in a passsive mode, 
	// where it waits for a client's approach to make a connection
	if(listen(listenfd, 10) < 0){
		printf("Error: listen.\n");

		// EXIT FAILURE
		exit(1);
	}

	// -------------------- The Chatroom --------------------
	// If there has been no error so far, the chat server will be available.
	printf("======= WELCOME TO KALINKA'S CHATROOM! =======\n");

	//  "Infinite loop": responsible for communicating, receiving messages 
	// from A client and sending them to everyone else.
	while (1) {
		socklen_t cliLen = sizeof(client_addr);
		//  Responsible for extracting the first connection request on the 
		// queue of pending connections (sockfd), creating a new connected 
		// socket and returning a new file descriptor referring to that socket.
		connfd = accept(listenfd, (struct sockaddr*) &client_addr, &cliLen);

		//  If the maximum number of clients has not yet been reached, 
		// the connection is made; otherwise, the client will be disconnected.
		if (MAX_CLI == (cliCount + 1)) {
			printf("Connection rejected: chatroom full.\n");
			//print_ip_addr(cli_addr);
			close(connfd);
			continue;
		}

		// -------------------- Client Management --------------------
		//  Defines client settings, adds it to the queue, 
		// creates a thread and a new function to handle client 
		Client* cli = (Client*) malloc(sizeof(Client));
		cli->address = client_addr;
		cli->sockfd = connfd;
		cli->userID = userID++;

		// Adding client to the queue
		add_client(cli);
		// Starting a new thread in the calling process
		pthread_create(&tid, NULL, &handle_client, (void*) cli);

		// Reducing CPU usage
		sleep(1);
	}

	// EXIT SUCCESS
	return 0;
}
