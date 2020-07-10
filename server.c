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
#define NICK_LEN 50
#define CHANNEL_LEN 200
#define CHANNEL_NUM 10

/* Atomic objects are the only objects that are free from data races,
 that is, they may be modified by two threads concurrently or
 modified by one and read by another. */
static _Atomic unsigned int cliCount = 0;
static int userID = 0;

// Colors used in users nicknames: red, green, yellow, blue, magenta and cyan.
char usrColors[MAX_CLI + 1][11] = {"\033[1;31m", "\033[1;32m", "\033[01;33m", "\033[1;34m", "\033[1;35m", "\033[1;36m"};

// Default color is white.
const char defltColor[7] = "\033[0m";
const char serverMsgColor[10] = "\033[1;32m";

/*  Client structure:
 stores the address, its socket descriptor, the user ID and the nickname;
 makes client differentiation possible. */

typedef struct {
	struct sockaddr_in address;
	int sockfd;
	int userID;
	char color[10];
	char nick[NICK_LEN];
	char channel[200];
	int isAdmin;
	int isMuted;
} Client;

Client* clients[MAX_CLI];

/*
	Channels names are strings (beginning with a '&' or '#' character) of
   length up to 200 characters.  Apart from the the requirement that the
   first character being either '&' or '#'; the only restriction on a
   channel name is that it may not contain any spaces (' '), a control G
   (^G or ASCII 7), or a comma (',' which is used as a list item
   separator by the protocol).
*/

typedef struct {
	char chName[CHANNEL_LEN];
	char chMode[3];
	char inviteUser[MAX_CLI][NICK_LEN];
	int nroInvUser;
} Channel;

Channel channel_list[CHANNEL_NUM];

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
void send_message_to_channel(char* msg, int userID, char* channel, int leaveFlag) {
	pthread_mutex_lock(&clients_mutex);

	// TESTE DE FALHA DE CONEXÃO
	if (teste) {
		Client* cli = (Client*) malloc(sizeof(Client));
		cli->sockfd = 1234;
		cli->userID = 1;
		clients[1] = cli;
	}

	// if(strcmp(channel,channel_list[0].chName) == 0)
	// 	return;

	for (int i = 0; i < MAX_CLI; i++) {

		if (clients[i]) {
			if (clients[i]->userID != userID && strcmp(clients[i]->channel, channel) == 0) {
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
	for(k = 0; k < NICK_LEN; k++) {
		if(buffer[k] == ':') break;
		else n[k] = buffer[k];
	}
}

void get_substring(char* sub, char* msg, int commandLen, int maxLen) {
	for (int i = commandLen; i <= commandLen+maxLen; i++) {
		sub[i-commandLen] = msg[i];
	}
}

// Checks whether the channel name is valid.
int check_channel(char *channel){

	if(channel[0] != '&' && channel[0] != '#') return 0;

	for (int i = 0; i<CHANNEL_LEN; i++)
		if(channel[i] == ' ' || channel[i] == ',' || channel[i] == (char)7) return 0;
	return 1;
}

// Checks if there is already a user with the specified nickname on the specified channel.
int check_nick(char *nick, char*channel){

	for (int i = 0; i < MAX_CLI; i++)
		if (clients[i] && strcmp(channel, clients[i]->channel) == 0 && strcmp(nick, clients[i]->nick) == 0)
			return 0;

	return 1;

}

// Handles clients, assigns their values and joins the chat
void* handle_client(void* arg) {
	/* leaveFlag indicates whether the client is connected or if they wish
	 to leave the chatroom. It also indicates if there's an error, which would
	 indicate the client should be disconnected. */
	int leaveFlag = 0;

	char buffer[BUFFER_MAX] = {};
	char menuChannel[BUFFER_MAX] = {};
	char nick[NICK_LEN] = {};
	char msg[MSG_LEN] = {};
	char channel[200] = {};
	char mode[3] = {};

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
		sprintf(buffer, "%s%s entrou no servidor!%s", cli->color, cli->nick, defltColor);
		printf("%s", buffer);

		memset(buffer, '\0', BUFFER_MAX);
		strcpy(buffer, "> Para entrar em um canal basta digitar \"/join nome_do_canal\"!\n\n> Você pode entrar em um dos canais já existentes ou criar o seu próprio crinal (lembrando que que o nome do canal deve começar com '#'ou '&'e não pode conter ',' ou ' ' ou ASCII7)\n\n");
		write(cli->sockfd, buffer, strlen(buffer));
		int channel_id = 0;

		strcpy(menuChannel,"Lista de canais:\n");

		for(int i = 0; i < CHANNEL_NUM; i++) {

			memset(buffer, '\0', BUFFER_MAX);
			if(channel_list[i].chName[0] != '\0') {
				if(channel_list[i].chMode[0] == '\0')
					sprintf(buffer, "\t%d - %s\n", channel_id, channel_list[i].chName);
				else
					sprintf(buffer, "\t%d - %s (invite-only)\n", channel_id, channel_list[i].chName);

				strcat(menuChannel,buffer);
				channel_id++;
			}
		}
		sprintf(buffer, "%s\n", buffer);
		strcat(menuChannel, buffer);

		write(cli->sockfd, menuChannel, strlen(menuChannel));
	}

	memset(buffer, '\0', BUFFER_MAX);

	// Message exchange
	while(1) {

		if(leaveFlag) break;

		int receive = recv(cli->sockfd, buffer, NICK_LEN+MSG_LEN, 0);

		nick_trim(buffer, msg);

		// Checks if the client wants to leave the chatroom
		if(receive == 0 || strcmp(msg, " /quit\n") == 0 || feof(stdin)) {

			sprintf(buffer, "%s%s saiu do servidor.%s\n", serverMsgColor, cli->nick, defltColor);
			printf("%s", buffer);
			send_message_to_channel(buffer, cli->userID, cli->channel, 0);
			leaveFlag = 1;

		} else if(strcmp(msg, " /quitChannel\n") == 0) {

			if (cli->isAdmin) {
				
				int otherClients = 0;
				for (int i = 0; i < MAX_CLI; i++) {
					if (clients[i] && strcmp(clients[i]->channel, cli->channel) == 0) {
						otherClients = 1;
						sprintf(buffer, "%s- %s%s\n", serverMsgColor, clients[i]->nick, defltColor);
						write(cli->sockfd, buffer, strlen(buffer));
					}
				}

				if (!otherClients) {
					sprintf(buffer, "%sComo você era o único aqui, seu canal já era!%s\n", serverMsgColor, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));

					for (int i = 0; i < CHANNEL_NUM; i++) {
						if (strcmp(channel_list[i].chName, cli->channel) == 0) {
							// FUNÇÃO deleteChannel
							memset(channel_list[i].chName, '\0', CHANNEL_LEN);
							memset(channel_list[i].chMode, '\0', 3);

							for(int j = 0; j < CHANNEL_NUM; j++)
								memset(channel_list[i].inviteUser[j], '\0', NICK_LEN);

							channel_list[i].nroInvUser = 0;
						}
					}
				}

				if (otherClients) {
					// memset(buffer, '\0', strlen(buffer));
					sprintf(buffer, "%sQuem seria o novo admin do canal %s?%s\n", serverMsgColor, cli->channel, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));

					char newAdmin[NICK_LEN];
					strcpy(newAdmin, "default");
					int resp;

					memset(buffer, '\0', strlen(buffer));

					resp = recv(cli->sockfd, buffer, NICK_LEN+MSG_LEN, 0);
					nick_trim(buffer, newAdmin);

					// printf("recv - %d\n", resp);
					str_trim(newAdmin, strlen(newAdmin));
					printf("%s\n", newAdmin+1);

					int clientFound = 0;
					for (int i = 0; i < MAX_CLI; i++) {
						if (clients[i] && strcmp(newAdmin+1, clients[i]->nick) == 0 && strcmp(cli->channel, clients[i]->channel) == 0) {
							clients[i]->isAdmin = 1;
							clients[i]->isMuted = 0;

							sprintf(buffer, "%sAgora você é o admin! Lembre-se: com grandes poderes vêm grandes responsabilidades!\n\n%s", serverMsgColor, defltColor);
							write(clients[i]->sockfd, buffer, strlen(buffer));

							clientFound = 1;
							break;
						}
					}

					if (!clientFound) {
						sprintf(buffer, "%sCliente não encontrado! Tente novamente...\n\n%s", serverMsgColor, defltColor);
						write(cli->sockfd, buffer, strlen(buffer));
					}
				}
				

			}

			sprintf(buffer, "%s%s saiu do canal.%s\n", cli->color, cli->nick, defltColor);
			printf("%s", buffer);
			send_message_to_channel(buffer, cli->userID, cli->channel, 0);

			strcpy(cli->channel, "&default");
			sprintf(buffer, "%sVocê saiu do canal.%s\n", cli->color, defltColor);
			write(cli->sockfd, buffer, strlen(buffer));
			
			// write(cli->sockfd, menuChannel, strlen(menuChannel));
			
		// Checks if the client wants to join some channel
		} else if(strncmp(msg, " /join", 6) == 0) {

			get_substring(channel, msg, 7, CHANNEL_LEN);
			str_trim(channel, strlen(channel));

			int publicChannel = 1;
			int invitedUser = 0;

			// Checking if it's an invite-only channel and, if so, if the client
			//was invited to it
			for(int i = 0; i < CHANNEL_NUM; i++){
				if(strcmp(channel_list[i].chName, channel) == 0 &&
				        strcmp(channel_list[i].chMode, "+i") == 0 ){

					publicChannel = 0;

					for(int j = 0; j < MAX_CLI; j++){
						if(strcmp(channel_list[i].inviteUser[j], cli->nick) == 0){
							invitedUser = 1;
						}

						break;
					}

					break;
				}
			}

			// Dealing with the impossibility of joining the channel
			if(!check_channel(channel) || !check_nick(cli->nick, channel) ||
		       (!publicChannel && !invitedUser)){

				memset(buffer, '\0', BUFFER_MAX);

				// If the channel is invalid
				if(!check_channel(channel)){
					sprintf(buffer, "%s\nInsira um nome de canal válido!\n\n%s", serverMsgColor, defltColor);
				}
				// If the user already participates in that channel
				else if(strcmp(cli->channel,channel) == 0){
					sprintf(buffer, "%s\nVocê já está neste canal!\n\n%s", serverMsgColor, defltColor);
				}
				// If it is an invite-only channel and the user has not been invited
				else if(!publicChannel && !invitedUser){
					sprintf(buffer, "%s\nDesculpe... Este é um canal invite-only e você não foi convidado.\n\n%s", serverMsgColor, defltColor);
				}
				// If there is already an user with that nickname on the channel
				else{
					sprintf(buffer, "%s\nJá existe um usuário com nickname %s nesse chat, para entrar mude seu nick com o comando: \"/nickname novo_nick\"!\n\n%s", serverMsgColor, cli->nick, defltColor);
				}

				write(cli->sockfd, buffer, strlen(buffer));

			// Dealing with the possibility of joining the channel
			} else{

				// Design decision: the user can only participate in one channel
				//at a time, so he cannot switch channels unless he disconnects
				//from the current one
				if (strcmp(cli->channel, channel_list[0].chName) != 0) {
					sprintf(buffer, "%s\nNada de ficar mudando de sala! Sem bagunça no KalinkUOL! Saia do servidor e entre novamente para poder se juntar a um outro canal.\n\n%s", serverMsgColor, defltColor);

					write(cli->sockfd, buffer, strlen(buffer));

				// If the user is not active on any specific channel yet (that
				//is, he is on the default channel) then he can join any
				} else {
					strcpy(cli->channel, channel);

					// Checks if channel requested already exists
					int newChannel = 1;
					int channelAvailable = 0;

					for (int i = 0; i < CHANNEL_NUM; i++) {
						if (channel_list[i].chName[0] == '\0') {
							channelAvailable++;
						}

						// If the channel already exists, the user is inserted
						//into it as a regular one (that is, he will not be an administrator)
						else if (strcmp(channel, channel_list[i].chName) == 0) {
							newChannel = 0;
							cli->isAdmin = 0;
							memset(buffer, '\0', BUFFER_MAX);
							sprintf(buffer, "%s\nBem-vindo ao canal %s, vulgo melhor canal!\n\n%s",serverMsgColor, channel, defltColor);
							write(cli->sockfd, buffer, strlen(buffer));
							break;
						}
					}

					// If channel does not exist
					if (newChannel) {
						// And if there's room available for one more channel, a
						//new channel will be created and the user will be the
						//administrator.
						if (channelAvailable > 0) {
							for (int i = 0; i < CHANNEL_NUM; i++) {
								if (channel_list[i].chName[0] == '\0') {
									strcpy(channel_list[i].chName, channel);
									cli->isAdmin = 1;
									memset(buffer, '\0', BUFFER_MAX);

									sprintf(buffer, "%s\nBem-vindo ao canal %s. Você é o admin! Lembre-se: com grandes poderes vêm grandes responsabilidades!\n\n%s",serverMsgColor, channel, defltColor);
									write(cli->sockfd, buffer, strlen(buffer));
									break;
								}
							}

						// If there's no room available...
						} else if (channelAvailable == 0) {
							memset(buffer, '\0', BUFFER_MAX);
							sprintf(buffer, "%s\nNão há espaço para novos canais!\n\n%s", serverMsgColor, defltColor);
							strcpy(cli->channel,channel_list[0].chName);
							write(cli->sockfd, buffer, strlen(buffer));
						}
					}

					//  Notifies other clients that this client has joined the channel
					sprintf(buffer, "\n%s%s entrou no canal %s!%s\n\n", cli->color, cli->nick, cli->channel, defltColor);
					printf("%s", buffer);

					send_message_to_channel(buffer, cli->userID, cli->channel, 0);
				}
			}
		} else if(strcmp(msg, " /ping\n") == 0) {

			char reply[5] = "pong\n";
			write(cli->sockfd, reply, strlen(reply));

		} else if(strncmp(msg, " /nickname", 10) == 0) {

			memset(nick, '\0', NICK_LEN);
			get_substring(nick, msg, 11, NICK_LEN);
		  	str_trim(nick, NICK_LEN);

			memset(buffer, '\0', BUFFER_MAX);
			sprintf(buffer, "\n%s%s agora se chama %s!\n\n%s", cli->color, cli->nick, nick, defltColor);
			printf("%s", buffer);
			send_message_to_channel(buffer, cli->userID, cli->channel, 0);

			strcpy(cli->nick, nick);

			memset(buffer, '\0', BUFFER_MAX);
			sprintf(buffer, "\n%sNick alterado para %s!\n\n%s", serverMsgColor, cli->nick, defltColor);
			write(cli->sockfd, buffer, strlen(buffer));

		} else if(strncmp(msg, " /kick", 6) == 0) {

			if(cli->isAdmin) {
				get_substring(nick, msg, 7, NICK_LEN);
				str_trim(nick, NICK_LEN);

				int clientFound = 0;

				for (int i = 0; i < MAX_CLI; i++) {

					if (clients[i] && strcmp(nick, clients[i]->nick) == 0) {
						strcpy(clients[i]->channel, channel_list[0].chName);

						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "\n%sVocê foi eliminado da casa do Big Kalinka Brasil.\n\n%s", serverMsgColor, defltColor);
						write(clients[i]->sockfd, buffer, strlen(buffer));

						sleep(0.7);

						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "\n%s/kicked\n\n%s", serverMsgColor, defltColor);
						write(clients[i]->sockfd, buffer, strlen(buffer));

						close(clients[i]->sockfd);
						remove_client(clients[i]->userID);
						cliCount--;
						free(clients[i]);

						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "\n%s%s não está mais espalhando seu fedor no canal!\n\n%s", serverMsgColor, nick, defltColor);
						printf("%s", buffer);
						write(cli->sockfd, buffer, strlen(buffer));

						clientFound = 1;
						break;
					}
				}

				if (!clientFound) {
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "\n%sCliente %s não encontrado.\n\n%s", serverMsgColor, nick, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}

			} else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "\n%sTá achando que aqui é casa da mãe Joana?\nSe quer kickar geral, cria seu próprio canal!\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}

		} else if(strncmp(msg, " /mute", 6) == 0) {

			if(cli->isAdmin) {
				get_substring(nick, msg, 7, NICK_LEN);
				str_trim(nick, NICK_LEN);

				int clientFound = 0;

				for (int i = 0; i < MAX_CLI; i++) {

					if (clients[i] && strcmp(nick, clients[i]->nick) == 0) {

						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "\n%sShh, cala boquinha.\n\n%s", serverMsgColor, defltColor);
						write(clients[i]->sockfd, buffer, strlen(buffer));

						clients[i]->isMuted = 1;

						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "\n%s%s foi silenciadah!\n\n%s", serverMsgColor, nick, defltColor);
						printf("%s", buffer);
						write(cli->sockfd, buffer, strlen(buffer));

						clientFound = 1;
						break;
					}
				}

				if (!clientFound) {
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "\n%sCliente %s não encontrado.\n\n%s", serverMsgColor, nick, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}
			}
			else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "\n%sTá achando que aqui é casa da mãe Joana?\nSe quer mutar geral, cria seu próprio canal!\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}

		} else if(strncmp(msg, " /unmute", 8) == 0) {

			if(cli->isAdmin) {
				get_substring(nick, msg, 9, NICK_LEN);
				str_trim(nick, NICK_LEN);

				int clientFound = 0;

				for (int i = 0; i < MAX_CLI; i++) {

					if (clients[i] && strcmp(nick, clients[i]->nick) == 0) {

						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "\n%sTá, pode falar.\n\n%s", serverMsgColor, defltColor);
						write(clients[i]->sockfd, buffer, strlen(buffer));

						clients[i]->isMuted = 0;

						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "\n%s%s foi liberadah!\n\n%s", serverMsgColor, nick, defltColor);
						printf("%s", buffer);
						write(cli->sockfd, buffer, strlen(buffer));

						clientFound = 1;
						break;
					}
				}

				if (!clientFound) {
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "\n%sCliente %s não encontrado.\n\n%s", serverMsgColor, nick, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}
			} else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "\n%sTá achando que aqui é casa da mãe Joana?\nPode sair desmutando assim não!\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}

		} else if(strncmp(msg, " /whois", 7) == 0) {

			if(cli->isAdmin) {
				get_substring(nick, msg, 8, NICK_LEN);
				str_trim(nick, NICK_LEN);

				int clientFound = 0;

				for (int i = 0; i < MAX_CLI; i++) {

					if (clients[i] && strcmp(nick, clients[i]->nick) == 0) {

						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "\n%sO endereço de IP de %s é %s\n\n%s", serverMsgColor, clients[i]->nick,inet_ntoa(clients[i]->address.sin_addr), defltColor);
						write(cli->sockfd, buffer, strlen(buffer));
						clientFound = 1;
						break;
					}
				}

				if (!clientFound) {

					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "\n%sCliente %s não encontrado.\n\n%s", serverMsgColor, nick, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}

			}else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "\n%sTá achando que aqui é casa da mãe Joana?\nPode sair querendo saber os IP dos outros assim não!\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}


		} else if(strncmp(msg, " /mode", 6) == 0) {

			if(cli->isAdmin) {
				get_substring(mode, msg, 7, 3);
				str_trim(mode, 3);

				// Finding the channel for which the administrator is responsible
				for (int i = 0; i < CHANNEL_NUM; i++) {
					if (strcmp(channel_list[i].chName, cli->channel) == 0) {
						strcpy(channel_list[i].chMode, mode);

						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "%s\nEste canal agora é invite-only!\n\n%s", serverMsgColor, defltColor);
						write(cli->sockfd, buffer, strlen(buffer));

						break;
					}
				}
			}else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "%s\nPoxa... Somente o administrador possui o direito de mudar o mode do canal.\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}


		} else if(strncmp(msg, " /invite", 8) == 0) {

			if(cli->isAdmin) {
				get_substring(nick, msg, 9, NICK_LEN);
				str_trim(nick, NICK_LEN);

				int clientFound = 0;
				int clientExists = 0;
				int idChannel = 0;
				int publicChannel = 1;

				// Finding the channel for which the administrator is responsible
				for (int i = 0; i < CHANNEL_NUM; i++) {
					if (strcmp(channel_list[i].chName, cli->channel) == 0) {
						idChannel = i;

						if(channel_list[i].chMode[0] != '\0'){
							publicChannel = 0;
						}

						break;
					}
				}

				if(publicChannel){
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "\n%sNão é possível convidar alguém para um canal que não é invite-only.\n\n%s", serverMsgColor, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}
				else{
					// Checking if the user exists and if isn't already invited to that channel
					for (int i = 0; i < MAX_CLI; i++) {

						if (clients[i] && strcmp(nick, clients[i]->nick) == 0)
							clientExists = 1;

						if (strcmp(channel_list[idChannel].inviteUser[i], nick) == 0) {
							memset(buffer, '\0', BUFFER_MAX);
							sprintf(buffer, "\n%sO usuário %s já foi convidado a se juntar a este chat.\n\n%s", serverMsgColor, nick, defltColor);
							write(cli->sockfd, buffer, strlen(buffer));

							break;
						}
					}

					// If the user has not yet been invited and it is possible to
					//invite more users to the chat, the process is done
					if(clientExists && !clientFound && channel_list[idChannel].nroInvUser < MAX_CLI - 1){
						for(int i = 0; i < MAX_CLI; i++){
							if(channel_list[idChannel].inviteUser[i][0] == '\0'){
								strcpy(channel_list[idChannel].inviteUser[i], nick);

								memset(buffer, '\0', BUFFER_MAX);
								sprintf(buffer, "\n%sO usuário %s foi convidado a se juntar a este chat.\n\n%s", serverMsgColor, nick, defltColor);
								write(cli->sockfd, buffer, strlen(buffer));

								channel_list[idChannel].nroInvUser++;

								break;
							}
						}

					}
					else if(!clientExists){
						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "\n%sO usuário precisa estar conectado ao servidor para poder ser convidado a participar deste canal.%s\n\n", serverMsgColor, defltColor);
						write(cli->sockfd, buffer, strlen(buffer));
					}
					else if(channel_list[idChannel].nroInvUser >= MAX_CLI - 1){
						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "\n%sO canal já atingiu o número máximo de usuários convidados.%s\n\n", serverMsgColor, defltColor);
						write(cli->sockfd, buffer, strlen(buffer));
					}
				}
			}
			else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "\n%sPoxa... Somente o administrador pode convidar usuários para este canal.\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}

		}else if(receive > 0) {

			if(strlen(buffer) > 0) {
				// str_overwrite_stdout();

				char n[NICK_LEN];
				change_color(buffer, n);
				snprintf(buffer, strlen(n)+strlen(buffer)+19, "%s%s%s:%s", cli->color, n, defltColor, msg);

				if (cli->isMuted == 0)
					send_message_to_channel(buffer, cli->userID, cli->channel, 0);


				// printf("%s%s%s", cli->color, buffer, defltColor);

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

	char* IP = "0.0.0.0";
	int port = 8192;

	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in server_addr, client_addr;
	pthread_t tid;


	for (int i = 0; i < MAX_CLI; i++) {
		memset(channel_list[i].chName, '\0', CHANNEL_LEN);
		memset(channel_list[i].chMode, '\0', 3);

		for(int j = 0; j < CHANNEL_NUM; j++){
			memset(channel_list[i].inviteUser[j], '\0', NICK_LEN);
		}

		channel_list[i].nroInvUser = 0;
	}

	strcpy(channel_list[0].chName, "&default");

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
		strcpy(cli->channel, channel_list[0].chName);
		cli->isMuted = 0;

		add_client(cli);
		// Starts a new thread in the calling process
		pthread_create(&tid, NULL, &handle_client, (void*) cli);

		// Reducing CPU usage
		sleep(1);
	}

	// EXIT SUCCESS
	return 0;
}