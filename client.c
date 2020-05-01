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

#define BUFFER_LEN 2049	//	Define where the split'll occur
#define SIZE_COLORS 19
#define BUFFER_MAX 4097
#define NICK_LEN 16

//  The value of a volative variable may change at any time,
// without any action being taken by the code the compiler finds nearby
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
	for (int i = 0; i < len; i++) {
		if (arr[i] == '\n') {
			arr[i] = '\0';
			break;
		}
	}
}

// Checks if the client wants to exit the program
void catch_ctrlC_and_exit() {
	leaveFlag = 1;
}

// Deals with receiving messages
void receive_message_handler() {
	char msg[NICK_LEN + BUFFER_LEN + SIZE_COLORS] = {};

	// While there are messages to be received
	while (1) {
		int rcv = recv(sockfd, msg, NICK_LEN+BUFFER_LEN + SIZE_COLORS, 0);

		// If something was written
		if (rcv > 0) {
			printf("%s", msg);
			str_overwrite_stdout();
		} else if (rcv == 0) {	// An error occurred
			break;
		}

		memset(msg, '\0', BUFFER_LEN + SIZE_COLORS + NICK_LEN);

		sleep(0.7);
	}
}

void lerString(char* buffer) {
  
    char c;
    int cont = 0;

    c = getchar();

    while (c != '\n') {
	    buffer[cont] = c;
	    c = getchar();
	    cont++;
    }

    buffer[cont] = '\0';
    cont++;
}

// Dealing with sending messages
void send_message_handler(){	
  char buffer[BUFFER_MAX] = {};
  char msg[BUFFER_MAX + NICK_LEN] = {};
  char sub[BUFFER_MAX] = {};

  // While there's no errors and the chat is running
  while(1){
  	bzero(sub, strlen(sub));

    str_overwrite_stdout();

    fgets(buffer, BUFFER_MAX, stdin); // Receives the message

    if(strcmp(buffer, "/quit\n") == 0){
    	leaveFlag = 1;

    	break;
    } 
    
    int j = 0;
    int tam = BUFFER_LEN;

    if(strlen(buffer) > tam){
      while(j < strlen(buffer)){
        int c = 0;
        while (c < tam && j < strlen(buffer)) {
          sub[c] = buffer[j];

          c++; j++;
        }
             
        str_trim(sub, BUFFER_MAX);

      	sprintf(msg, "%s: %s\n", nick, sub);
      	send(sockfd, msg, strlen(msg), 0);

      	sleep(0.7);

        memset(sub, '\0', BUFFER_LEN);
        memset(msg, '\0', BUFFER_LEN + NICK_LEN);
      }  
    }
    else{
      	str_trim(buffer, BUFFER_MAX);

        sprintf(msg, "%s: %s\n", nick,buffer);
        send(sockfd, msg, strlen(msg), 0);
    }

    bzero(buffer, BUFFER_MAX);
    bzero(msg, BUFFER_MAX+NICK_LEN);
  }

  catch_ctrlC_and_exit();
}

int main(int argc, char* const argv[]) {
	// if (argc != 2) {
	// 	printf("Erro. Try: %s <port>\n", argv[0]);
	// 	// EXIT FAILURE
	// 	return 1;
	// }

	char* IP = "127.0.0.1";
	int port = 1234;
	// int port = atoi(argv[1]);

	signal(SIGINT, catch_ctrlC_and_exit); // Interruption signal

	// Manages client's nickname
	printf("Qual o seu nick? ");
	fgets(nick, NICK_LEN + 2, stdin);
	str_trim(nick, NICK_LEN);

	// Checks if the given nickname is valid
	if (strlen(nick) > NICK_LEN - 1 || strlen(nick) < 2) {
		printf("Digite um nick válido. O nick deve possuir de 2 a 15 caracteres.\n");

		// EXIT FAILURE
		exit(1);
	}

	for(int i = 0; i < strlen(nick); i++){
		if(nick[i] == ':'){
			printf("Digite um nick válido. O nick não pode possuir o caracter especial ':'.\n");

			// EXIT FAILURE
			exit(1);
		}
	}

	struct sockaddr_in server_addr;

	//  AF_INET is an address family that designate IPv4 as the address' 
	// type that the socket can communicate;
	//  SOCK_STREAM defines the communication type - in this case, TCP
	//  The third argument defines the protocol value for IP
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// IP and port bind and a connection will open based on both	
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(IP);
	server_addr.sin_port = htons(port);

	// -------------------- Connecting Client to Server --------------------
	int err = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)); // Used to create a connection to the 
																					//specified foreign association. The 
																					//parameter s specifies an unconnected 
																					//datagram or stream socket. 
	if (err == -1) {
		printf("Erro: connect.\n");

		// EXIT FAILURE
		exit(1);
	}

	//Sending the nickname to the server
	send(sockfd, nick, NICK_LEN, 0);

	// -------------------- The Chatroom --------------------
	//  If there has been no error so far, 
	// the client is now connected to the chat
	// printf("======= WELCOME TO KALINKA'S CHATROOM! =======\n");
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

	if (pthread_create(&sendMsgThread, NULL, (void*) send_message_handler, NULL) != 0) {
		printf("Erro: pthread.\n");

		// EXIT FAILURE
		exit(1);
	}

	pthread_t receiveMsgThread;

	if (pthread_create(&receiveMsgThread, NULL, (void*) receive_message_handler, NULL) != 0) {
		printf("Erro: pthread.\n");

		// EXIT FAILURE
		exit(1);
	}

	// Making the chat active and finalizing when it's the properly moment
	while (1) {
		if (leaveFlag) {
			break;
		}
	}

	// The client has left the chat, so...
	printf("\nTchau!\n");
	close(sockfd);

	// EXIT SUCCESS
	return 0;
}