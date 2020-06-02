/*
	Client - Major Steps:
	- Create a socket with socket();
	- Connect the socket to the address of the server using the connect() system call;
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

#define BUFFER_LEN 2049	//	Define where the split will occur
#define BUFFER_MAX 4097
#define NICK_LEN 16
#define SIZE_COLORS 19

/* The value of a volative variable may change at any time,
 without any action being taken by the code the compiler finds nearby. */
volatile sig_atomic_t leaveFlag = 0;

int sockfd = 0;
char nick[NICK_LEN];

// Responsible for overwriting and flushing the stdout
void str_overwrite_stdout() {
	printf("\r%s", "> ");
	fflush(stdout);
}

// Responsible for removing undesirable '\n'
void str_trim(char* arr, int len) {
	for(int i = 0; i < len; i++) {
		if(arr[i] == '\n') {
			arr[i] = '\0';
			break;
		}
	}
}

// Checks if the client wants to exit the program
void catch_ctrl_d_and_exit(int sig) {
	leaveFlag = 1;
}

// Deals with receiving messages
void receive_message_handler() {
	char msg[NICK_LEN+BUFFER_LEN+SIZE_COLORS] = {};

	// While there are messages to be received
	while(1) {
		int rcv = recv(sockfd, msg, NICK_LEN+BUFFER_LEN+SIZE_COLORS, 0);

		// If something was written
		if(rcv > 0) {
			printf("%s", msg);
			str_overwrite_stdout();
		} else if(rcv == 0) {	// An error occurred
			break;
		}

		memset(msg, '\0', BUFFER_LEN+SIZE_COLORS+NICK_LEN);

		sleep(0.7);
	}
}

/* If message is longer than the maximum length permitted,
 it is split in multiple messages. */
void split_message(char *buffer, char *msg) {
	int j = 0;
	char sub[BUFFER_MAX] = {};

	while(j < strlen(buffer)) {
    	int c = 0;
        while(c < BUFFER_LEN && j < strlen(buffer)) {
        	sub[c] = buffer[j];
      		c++; j++;
    	}

	    str_trim(sub, BUFFER_MAX);

	  	sprintf(msg, "%s: %s\n", nick, sub);
	  	send(sockfd, msg, strlen(msg), 0);

	  	sleep(0.7);

	    memset(sub, '\0', BUFFER_LEN);
	    memset(msg, '\0', BUFFER_LEN+NICK_LEN);
	}
}

// Dealing with sending messages
void send_message_handler() {
	char buffer[BUFFER_MAX] = {};
	char msg[BUFFER_MAX+NICK_LEN] = {};

	// While there's no errors and the chat is running
	while(1) {
		str_overwrite_stdout();

		fgets(buffer, BUFFER_MAX, stdin); // Receives the message

		if(strcmp(buffer, "/quit\n") == 0 || feof(stdin)) {
			leaveFlag = 1;
			break;
		} else if(strlen(buffer) > BUFFER_LEN) {
			split_message(buffer, msg);
		} else {
		  	str_trim(buffer, BUFFER_MAX);
		    sprintf(msg, "%s: %s\n", nick, buffer);
		    send(sockfd, msg, strlen(msg), 0);
		}

		bzero(buffer, BUFFER_MAX);
		bzero(msg, BUFFER_MAX+NICK_LEN);
	}

	catch_ctrl_d_and_exit(2);
}

void input_nickname() {
	printf("Qual o seu nick? ");
	fgets(nick, NICK_LEN+2, stdin);
	str_trim(nick, NICK_LEN);

	// Checks if the given nickname is valid
	if(strlen(nick) > NICK_LEN - 1 || strlen(nick) < 2) {
		printf("\nDigite um nick válido.\nO nick deve possuir de 2 a 15 caracteres.\n");

		// EXIT FAILURE
		exit(1);
	}

	for(int i = 0; i < strlen(nick); i++) {
		if(nick[i] == ':') {
			printf("\nDigite um nick válido.\nO nick não pode possuir o caracter especial ':'.\n");

			// EXIT FAILURE
			exit(1);
		}
	}
}

int main(int argc, char* const argv[]) {
	// if(argc != 2) {
	// 	printf("Erro. Try: %s <port>\n", argv[0]);
	// 	// EXIT FAILURE
	// 	return 1;
	// }

	char* IP = "127.0.0.1";
	int port = 1234;
	// int port = atoi(argv[1]);

	// Ignores CTRL+C
	signal(SIGINT, SIG_IGN);
	// Sets CTRL+D to /quit
	signal(EOF, catch_ctrl_d_and_exit);

	input_nickname();

	struct sockaddr_in server_addr;

	/*  AF_INET is an address family that designates IPv4 as the address'
	 type that the socket can communicate;
	  SOCK_STREAM defines the communication type - in this case, TCP
	  The third argument defines the protocol value for IP. */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// IP and port bind and a connection will open based on both
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(IP);
	server_addr.sin_port = htons(port);

	// -------------------- Connecting Client to Server --------------------

	printf("\nPara entrar na sala, digite \"/connect\"!\n");
	char tmp[BUFFER_MAX];
	while(1) {
		fgets(tmp, BUFFER_MAX, stdin);
		if(strcmp(tmp, "/connect\n") == 0) break;
		else if (strcmp(tmp, "/quit\n") == 0) {
			printf("\nTchauzinho!\n");
			exit(1);
		} else {
			printf("\nOpa, você digitou um comando inválido!\n");
			exit(1);
		}
	}

	/* Used to create a connection to the specified foreign association.
	 The parameters specify an unconnected datagram or stream socket. */

	int err = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if(err == -1) {
		printf("\nErro: connect.\n");
		// EXIT FAILURE
		exit(1);
	}

	// Sending the nickname to the server
	send(sockfd, nick, NICK_LEN, 0);

	// --------------------------------------- The Chatroom --------------------------------------
	//  If there has been no error so far, the client is now connected to the chat

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

	// Defining two threads: one to receive messages and another to send messages
	pthread_t sendMsgThread;

	if(pthread_create(&sendMsgThread, NULL, (void*) send_message_handler, NULL) != 0) {
		printf("\nErro: pthread.\n");

		// EXIT FAILURE
		exit(1);
	}

	pthread_t receiveMsgThread;

	if(pthread_create(&receiveMsgThread, NULL, (void*) receive_message_handler, NULL) != 0) {
		printf("\nErro: pthread.\n");

		// EXIT FAILURE
		exit(1);
	}

	// Making the chat active and finalizing when it's the properly moment
	while(1) {
		if(leaveFlag) {
			break;
		}
	}

	// When client has left the chat
	printf("\nTchauzinho!\n");

	close(sockfd);

	// EXIT SUCCESS
	return 0;
}