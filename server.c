/*
	Server - Major Steps:
	- Create a socket with socket();
	- Bind the socket to an address using bind();
	- Listen for connections with listen();
	- Accept a connection with accept();
	- Send and receive data, using read() and write() system calls.
*/

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#define BUFFER_MAX 4097
#define MAX_CLI 6
#define MSG_LEN 2049
#define NICK_LEN 16

/* Atomic objects are the only objects that are free from data races,
 that is, they may be modified by two threads concurrently or
 modified by one and read by another. */
static _Atomic unsigned int cliCount = 0;
static int userID = 0;

// Colors used in users nicknames: red, green, yellow, blue, magenta and cyan.
char usrColors[MAX_CLI + 1][11] = {"\033[1;31m", "\033[1;32m", "\033[01;33m", "\033[1;34m", "\033[1;35m", "\033[1;36m"};
// Default color is white.
const char defltColor[7] = "\033[0m";

/*  Client structure:
 stores the address, its socket descriptor, the user ID and the nickname;
 makes client differentiation possible. */
typedef struct {
	struct sockaddr_in address;
	int sockfd;
	int userID;
	char color[10];
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
	for(int i = 0; i < len; i++) {
		if(arr[i] == '\n') {
			arr[i] = '\0';
			break;
		}
	}
}

// void catch_ctrl_d_and_exit(int sig, int) {
// 	connected = 0;
// }

// Adds clients to the array of clients
void add_client(Client* cli) {
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAX_CLI; i++) {
		if (!clients[i]) {
			clients[i] = cli;
			strcpy(clients[i]->color, usrColors[i]);

			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Removes clients from the array of clients
void remove_client(int userID) {
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAX_CLI; i++) {
		if(clients[i]) {
			if(clients[i]->userID == userID) {
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}


int teste = 0;

// Sends messages to all the clients, except the sender itself
void send_message_to_all(char* msg, int userID, int leaveFlag) {
	pthread_mutex_lock(&clients_mutex);

	// TESTE DE FALHA DE CONEXÃO
	if (teste) {
		Client* cli = (Client*) malloc(sizeof(Client));
		cli->sockfd = 1234;
		cli->userID = 1;
		clients[1] = cli;
	}

	for (int i = 0; i < MAX_CLI; i++) {
		
		if (clients[i]) {
			if (clients[i]->userID != userID) {
				sleep(0.7);

				int counter = 0;
				while (write(clients[i]->sockfd, msg, strlen(msg)) < 0) {

					if (counter == 4) {
						printf("Erro: a mensagem não pode ser enviada.\n");
						
						pthread_mutex_unlock(&clients_mutex);
						remove_client(i);
						pthread_mutex_lock(&clients_mutex);

						break;
					}

					counter++;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Separates nick and message in the incoming buffer
void nick_trim(char* buffer, char* msg) {
	for(int j = 0; j < NICK_LEN; j++) {
		if(buffer[j] == ':') {
			strcpy(msg, buffer+j+1);
			break;
		}
	}
}

void change_color(char *buffer, char *n) {
	int k;

	memset(n, '\0', NICK_LEN);
	for(k = 0; k < NICK_LEN; k++){
		if(buffer[k] == ':') break;
		else n[k] = buffer[k];
	}
}

// Handles clients, assigns their values and joins the chat
void* handle_client(void* arg) {
	/* leaveFlag indicates whether the client is connected or if they wish
	 to leave the chatroom. It also indicates if there's an error, which would
	 indicate the client should be disconnected. */
	int leaveFlag = 0;

	char buffer[BUFFER_MAX] = {};
	char nick[NICK_LEN] = {};
	char msg[MSG_LEN] = {};

	cliCount++;

	Client* cli = (Client*) arg;

	/* Naming the client:
	 recv() is used to receive messages from a socket;
	 Nicknames must be at least 3 characters long
	 and should not exceed the maximum length established above.*/
	if(recv(cli->sockfd, nick, NICK_LEN, 0) <= 0 || strlen(nick) < 2 || strlen(nick) > NICK_LEN - 1) {
		printf("\nErro: nick inválido.\n");
		leaveFlag = 1;
	} else {
		strcpy(cli->nick, nick);
		//  Notifies other clients that this client has joined the chatroom
		sprintf(buffer, "%s%s entrou!%s\n", cli->color, cli->nick, defltColor);  // sprintf function is used to store formatted data as a string
		printf("%s", buffer);

		send_message_to_all(buffer, cli->userID, 0);
	}

	memset(buffer, '\0', BUFFER_MAX);

	// Message exchange
	while(1) {

		if(leaveFlag) break;

		int receive = recv(cli->sockfd, buffer, NICK_LEN+MSG_LEN, 0);

		// Checks if the client wants to leave the chatroom
		nick_trim(buffer, msg);
		if(receive == 0 || strcmp(msg, " /quit\n") == 0 || feof(stdin)) {

			sprintf(buffer, "%s%s saiu.%s\n", cli->color, cli->nick, defltColor);
			printf("%s", buffer);
			send_message_to_all(buffer, cli->userID, 0);
			leaveFlag = 1;

		} else if(strcmp(msg, " /ping\n") == 0) {

			char reply[5] = "pong\n";
			write(cli->sockfd, reply, strlen(reply));
		
		} else if(receive > 0) {

			if(strlen(buffer) > 0) {
				str_overwrite_stdout();

				char n[NICK_LEN];
				change_color(buffer, n);
				snprintf(buffer, strlen(n)+strlen(buffer)+19, "%s%s%s:%s", cli->color, n, defltColor, msg);

				send_message_to_all(buffer, cli->userID, 0);

				printf("%s%s%s", cli->color, buffer, defltColor);

				bzero(buffer, BUFFER_MAX);
			}
		} else {
			printf("\nErro.\n");
			leaveFlag = 1;
		}

		memset(buffer, '\0', BUFFER_MAX);
		memset(msg, '\0', MSG_LEN);

		sleep(0.7);
	}

	// When client leaves the chat
	close(cli->sockfd);
	remove_client(cli->userID);
	cliCount--;
	free(cli);
	// Marks the thread identified by thread as detached
	pthread_detach(pthread_self());

	return NULL;
}


int main(int argc, char* const argv[]) {
	// if(argc != 2) {
	// 	printf("Erro. Tente: %s <port>\n", argv[0]);

	// 	// EXIT FAILURE
	// 	return 1;
	// }

	char* IP = "127.0.0.1";
	int port = 1234;
	// int port = atoi(argv[1]);

	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in server_addr, client_addr;
	pthread_t tid;

	// signal(EOF, catch_ctrl_d_and_exit);

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
	if(setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*) &option, sizeof(option)) < 0) {
		printf("\nErro: setsockopt.\n");

		// EXIT FAILURE;
		exit(1);
	}

	/* After creating the socket, the bind() function binds the
	 socket to the address and the port number specified in addr. */
	if(bind(listenfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
		printf("\nErro: bind.\n");

		// EXIT FAILURE
		exit(1);
	}

	/* The listen() function puts the server socket in a passsive mode,
	 where it waits for a client's approach to make a connection. */
	if(listen(listenfd, 10) < 0){
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
	// int connected = 1;
	// while(connected) {
	while(1) {

		socklen_t cliLen = sizeof(client_addr);
		/* Responsible for extracting the first connection request on the
		 queue of pending connections, creating a new connected socket
		 and returning a new file descriptor referring to that socket. */
		connfd = accept(listenfd, (struct sockaddr*) &client_addr, &cliLen);

		/*  If the maximum number of clients has not yet been reached,
		 the connection is made; otherwise, the client will be disconnected. */
		if(MAX_CLI < (cliCount + 1)) {
			char tmp[70] = {"Opa, sala cheia! Quem sabe na próxima...\nPressione ENTER para sair.\n"};
			write(connfd, tmp, strlen(tmp));
			close(connfd);

			continue;
		}

		// -------------------- Client Management --------------------
		/*  Defines client settings, adds it to the queue,
		 creates a thread and a new function to handle client */
		Client* cli = (Client*) malloc(sizeof(Client));
		cli->address = client_addr;
		cli->sockfd = connfd;
		cli->userID = userID++;

		add_client(cli);
		// Starts a new thread in the calling process
		pthread_create(&tid, NULL, &handle_client, (void*) cli);

		// Reducing CPU usage
		sleep(1);

		// catch_ctrl_d_and_exit(2);
	}

	// EXIT SUCCESS
	return 0;
}