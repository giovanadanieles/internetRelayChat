/*
	Client - Major Steps:
	- Create a socket with the socket();
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

#define MAX_CLI 10
#define BUFFER_SZ 4096
#define NICK_LEN 16

volatile sig_atomic_t flag = 0;	// The value of a volative variable may change at any time,
								//without any action being taken by the code the compiler finds nearby
int sockfd = 0;
char nick[NICK_LEN];

// Responsible for overwrite and flush the stdout
void strOverwriteStdout(){
	printf("\r%s", "> ");
	fflush(stdout);
}

// Responsible for removing undesirables '\n'
void strTrimLF(char* arr, int len){
	for(int i = 0; i < len; i++){
		if(arr[i] == '\n'){
			arr[i] = '\0';
			break;
		}
	}
}

//Checking if the client wants to exit the program
void catchCtrlCAndExit(){
	flag = 1;
}

// Dealing with receiving messages
void receiveMessageHandler(){
	char msg[BUFFER_SZ] = {};

	// While there are messages to be received
	while(1){
		int rcv = recv(sockfd, msg, BUFFER_SZ, 0);

		// If there's something wrote
		if(rcv > 0){
			printf("%s", msg);
			strOverwriteStdout();
		}
		else if(rcv == 0){	// An error occurred
			break;
		}

		bzero(msg, BUFFER_SZ);
	}
}

// Dealing with sending messages
void sendMessageHandler(){
	char buffer[BUFFER_SZ] = {};
	char msg[BUFFER_SZ + NICK_LEN] = {};

	// While there's no errors and the chat is running
	while(1){
		strOverwriteStdout();
		fgets(buffer, BUFFER_SZ, stdin); // Receives the message
		strTrimLF(buffer, BUFFER_SZ);

		if(strcmp(buffer, "exit") == 0) break;
		else{
			sprintf(msg, "%s: %s\n", nick, buffer);
			send(sockfd, msg, strlen(msg), 0);
		}

		bzero(buffer, BUFFER_SZ);
		bzero(msg, BUFFER_SZ+NICK_LEN);
	}

	catchCtrlCAndExit(2);
}

int main(int argc, char* const argv[]){
	if(argc != 2){
		printf("Error. Try: %s <port>\n", argv[0]);

		// EXIT FAILURE
		return 1;
	}

	char* IP = "127.0.0.1";
	int port = atoi(argv[1]);

	signal(SIGINT, catchCtrlCAndExit); // Interruption signal

	// Managing the nickname of the client
	printf("Enter your name: ");
	fgets(nick, NICK_LEN, stdin);
	strTrimLF(nick, NICK_LEN);

	// Checking if the given nickname is valid
	if(strlen(nick) > NICK_LEN - 1 || strlen(nick) < 2){
		printf("Enter a valid nickname.\n");

		// EXIT FAILURE
		exit(1);
	}

	struct sockaddr_in server_addr;

	// AF_INET is an address family that designate IPv4 as the address' type that the socket can communicate;
	// SOCK_STREAM defines the communication type - in this case, TCP
	// The third argument defines the protocol value for IP
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// IP and port bind and a connection will open based on both	
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(IP);
	server_addr.sin_port = htons(port);

	// ----------------------------------------------------------------------------------------------------------------------  Connecting Client to Server
	int err = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)); // Used to create a connection to the 
																					//specified foreign association. The 
																					//parameter s specifies an unconnected 
																					//datagram or stream socket. 
	if(err == -1){
		printf("Error: connect.\n");

		// EXIT FAILURE
		exit(1);
	}

	//Sending the nickname to the server
	send(sockfd, nick, NICK_LEN, 0);

	// ----------------------------------------------------------------------------------------------------------------------  The Chatroom
	// If there has been no error so far, the client is now connected to the chat
	printf("======= WELCOME TO THE CHATROOM! =======\n");

	// Defining two threads: one to receive messages and another to send messages
	pthread_t sendMsgThread;

	if(pthread_create(&sendMsgThread, NULL, (void*)sendMessageHandler, NULL) != 0){
		printf("Error: pthread.\n");

		// EXIT FAILURE
		exit(1);
	}

	pthread_t receiveMsgThread;

	if(pthread_create(&receiveMsgThread, NULL, (void*)receiveMessageHandler, NULL) != 0){
		printf("Error: pthread.\n");

		// EXIT FAILURE
		exit(1);
	}

	// Making the chat active and finalizing when it's the properly moment
	while(1){
		if(flag){
			printf("\nBye!\n");
			break;
		}
	}

	// The client has left the chat, so...
	close(sockfd);

	// EXIT SUCCESS
	return 0;
}
