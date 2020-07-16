#include "server_operation.h"

/* Atomic objects are the only objects that are free from data races,
 that is, they may be modified by two threads concurrently or
 modified by one and read by another. */
static _Atomic unsigned int cliCount = 0;
static int userID = 0;

// Colors used in users nicknames: red, green, yellow, blue, magenta and cyan.
char usrColors[7][11] = {"\033[1;31m", "\033[1;32m", "\033[01;33m", "\033[1;34m", "\033[1;35m", "\033[1;36m"};

// Default color is white.
const char defltColor[7] = "\033[0m";
const char serverMsgColor[10] = "\033[1;32m";

Client* clients[MAX_CLI];

Channel channel_list[CHANNEL_NUM];

// Necessary to send messages between the clients
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// === FUNCTIONS RELATED TO SERVER OPERATION ===

/* If the maximum number of clients has not yet been reached,
the connection is made; otherwise, the client will be disconnected. */
void is_server_full(int connfd) {
	if(MAX_CLI < (cliCount + 1)) {
		char tmp[70] = {"Opa, sala cheia! Quem sabe na próxima...\nPressione ENTER para sair.\n"};
		write(connfd, tmp, strlen(tmp));
		close(connfd);

		return;
	}
}

// Adds clients to the array of clients.
void add_client(Client* cli) {
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAX_CLI; i++) {
		if (!clients[i]) {
			clients[i] = cli;
			strcpy(clients[i]->color, usrColors[i%7]);

			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

// Creates client structure.
void create_client(struct sockaddr_in client_addr, int connfd, Client* cli) {
	// Client* cli = (Client*) malloc(sizeof(Client));
	cli->address = client_addr;
	cli->sockfd = connfd;
	cli->userID = userID++;
	strcpy(cli->channel, channel_list[0].chName);
	cli->isMuted = 0;

	add_client(cli);

	// return cli;
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

// Sends messages to all the clients, except the sender itself
void send_message_to_channel(char* msg, int userID, char* channel, int leaveFlag) {
	pthread_mutex_lock(&clients_mutex);

	// TESTE DE FALHA DE CONEXÃO
	int teste = 0;
	if (teste) {
		Client* cli = (Client*) malloc(sizeof(Client));
		cli->sockfd = 1234;
		cli->userID = 1;
		clients[1] = cli;
	}

	for (int i = 0; i < MAX_CLI; i++) {

		if (clients[i]) {
			if (clients[i]->userID != userID && strcmp(clients[i]->channel, channel) == 0) {
				sleep(0.7);

				int counter = 0;
				while (write(clients[i]->sockfd, msg, strlen(msg)) < 0) {
					printf("mandando\n");

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

// Checks whether the channel name is valid.
int check_channel(char *channel) {

	if(channel[0] != '&' && channel[0] != '#') return 0;

	for (int i = 0; i<CHANNEL_LEN; i++)
		if(channel[i] == ' ' || channel[i] == ',' || channel[i] == (char)7) return 0;
	return 1;
}

// Checks if there is already a user with the specified nickname on the specified channel.
int check_nick(char* nick, char* channel) {

	for (int i = 0; i < MAX_CLI; i++)
		if (clients[i] && strcmp(channel, clients[i]->channel) == 0 && strcmp(nick, clients[i]->nick) == 0)
			return 0;

	return 1;
}

// Creates initial channel list.
void initialize_channel_list() {
	for (int i = 0; i < CHANNEL_NUM; i++) {
			memset(channel_list[i].chName, '\0', CHANNEL_LEN);
			strcpy(channel_list[i].chMode, "-i");

			clear_invite_list(i);

			channel_list[i].nroInvUser = 0;
		}

	strcpy(channel_list[0].chName, "&default");
}

// Shows channel menu.
void channel_menu(Client* cli) {
	char buffer[BUFFER_MAX] = {};
	char menuChannel[BUFFER_MAX] = {};

	int channel_id = 0;

	memset(buffer, '\0', BUFFER_MAX);
	strcpy(buffer, "Para entrar em um canal basta digitar \"/join nome_do_canal\"!\n\n> Você pode entrar em um dos canais já existentes ou criar o seu próprio crinal (lembrando que que o nome do canal deve começar com '#'ou '&'e não pode conter ',' ou ' ' ou ASCII7)\n\n");
	write(cli->sockfd, buffer, strlen(buffer));

	strcpy(menuChannel, "Lista de canais:\n");

	for(int i = 0; i < CHANNEL_NUM; i++) {

		memset(buffer, '\0', BUFFER_MAX);

		if(channel_list[i].chName[0] != '\0') {

			if(strcmp(channel_list[i].chMode,"+i") != 0)
				sprintf(buffer, "\t%d - %s\n", channel_id, channel_list[i].chName);
			else
				sprintf(buffer, "\t%d - %s (invite-only)\n", channel_id, channel_list[i].chName);

			strcat(menuChannel, buffer);
			channel_id++;
		}
	}

	write(cli->sockfd, menuChannel, strlen(menuChannel));

}

// Shows welcome menu
void welcome_menu(Client* cli) {

	char buffer[BUFFER_MAX] = {};

	memset(buffer, '\0', BUFFER_MAX);
	strcpy(buffer, "Comandos gerais:\t\tComandos de administrador:\n- /join <nomeCanal>\t\t- /kick <nomeUsuario>\n- /nickname <novoNick>\t\t- /mute <nomeUsuario>\n- /ping\t\t\t\t- /unmute <nomeUsuario>\n- /quit\t\t\t\t- /whois <nomeUsuario>\n- /quitchannel\t\t\t- /mode <+i|-i>\n \t\t\t\t- /invite <nomeUsuario>\n\n");
	write(cli->sockfd, buffer, strlen(buffer));

	channel_menu(cli);
}

// Handles client leaving channel.
void client_leaves_channel(Client* cli) {
	char buffer[BUFFER_MAX] = {};

	if(strcmp(cli->channel, "&default") == 0){
		sprintf(buffer, "%sNão é possível deixar o canal &default.%s\n", serverMsgColor, defltColor);
		write(cli->sockfd, buffer, strlen(buffer));
	}
	else{
		sprintf(buffer, "%s%s saiu do canal.%s\n", cli->color, cli->nick, defltColor);
		printf("%s", buffer);
		send_message_to_channel(buffer, cli->userID, cli->channel, 0);

		strcpy(cli->channel, "&default");

		sprintf(buffer, "%sVocê saiu do canal.%s\n", cli->color, defltColor);
		write(cli->sockfd, buffer, strlen(buffer));

		channel_menu(cli);
	}
}

// Finds other clients in the same channel and list
int find_other_clients(Client* cli) {
	char buffer[BUFFER_MAX] = {};

	int otherClients = 0;

	for (int i = 0; i < MAX_CLI; i++) {

		if (clients[i] && clients[i]->userID != cli->userID && strcmp(clients[i]->channel, cli->channel) == 0) {
			otherClients = 1;
			sprintf(buffer, "%s- %s%s\n", serverMsgColor, clients[i]->nick, defltColor);
			write(cli->sockfd, buffer, strlen(buffer));
		}
	}

	return otherClients;
}

// Deletes existing channel.
void delete_channel(Client* cli) {
    
	int idChannel = find_channel(cli);
	memset(channel_list[idChannel].chName, '\0', CHANNEL_LEN);
	strcpy(channel_list[idChannel].chMode, "-i");
	
	for(int j = 0; j < MAX_CLI; j++)
		memset(channel_list[idChannel].inviteUser[j], '\0', NICK_LEN);
	
	channel_list[idChannel].nroInvUser = 0;
   
}

// Changes channel's admin.
int change_admin(Client*cli) {
    
    char buffer[BUFFER_MAX] = {};
    
    sprintf(buffer, "%sDados os clientes acima, quem será o novo admin do canal %s?%s\n", serverMsgColor, cli->channel, defltColor);
    write(cli->sockfd, buffer, strlen(buffer));
    
    char newAdmin[NICK_LEN];
    strcpy(newAdmin, "default");
    
    memset(buffer, '\0', strlen(buffer));
    
    recv(cli->sockfd, buffer, NICK_LEN+MSG_LEN, 0);
    nick_trim(buffer, newAdmin);
    
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

	cli->isAdmin = 0;
    
    return clientFound;
}

// Finds clients in the same channel.
int find_client(char* nick,Client* cli) {
	
	for (int i = 0; i < MAX_CLI; i++) {

		if (clients[i] && strcmp(nick, clients[i]->nick) == 0 && strcmp(cli->channel,clients[i]->channel) == 0)  
			return i;
	}
	return -1;

}

// Finds current client's channel.
int find_channel(Client* cli) {

	for (int i = 0; i < CHANNEL_NUM; i++) {
		if (strcmp(channel_list[i].chName, cli->channel) == 0) 
			return i;	
	} 
	return -1;
}

// Clears the list of invited users for a given chat.
void clear_invite_list(int idChannel){
	for(int i = 0; i < CHANNEL_NUM; i++){
		memset(channel_list[idChannel].inviteUser[i], '\0', NICK_LEN);
	}
}

// Handles clients, assigns their values and joins the chat
void* handle_client(void* arg) {
	/* leaveFlag indicates whether the client is connected or if they wish
	 to leave the chatroom. It also indicates if there's an error, which would
	 indicate the client should be disconnected. */
	int leaveFlag = 0;

	char buffer[BUFFER_MAX] = {};
	// char menuChannel[BUFFER_MAX] = {};
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
		sprintf(buffer, "%s%s entrou no servidor!\n%s", cli->color, cli->nick, defltColor);
		printf("%s", buffer);

		welcome_menu(cli);
	}

	memset(buffer, '\0', BUFFER_MAX);

	// Message exchange
	while(1) {

		if(leaveFlag) break;

		int receive = recv(cli->sockfd, buffer, NICK_LEN+MSG_LEN, 0);
		printf("%s", buffer);

		nick_trim(buffer, msg);

		// Checks if the client wants to leave the chatroom
		if(receive == 0 || strcmp(msg, " /quit\n") == 0 || feof(stdin)) {

			sprintf(buffer, "%s%s saiu do servidor.%s\n", serverMsgColor, cli->nick, defltColor);
			printf("%s", buffer);
			send_message_to_channel(buffer, cli->userID, cli->channel, 0);
			leaveFlag = 1;

		} else if(strcmp(msg, " /quitchannel\n") == 0) {

			if (cli->isAdmin == 0) {

				client_leaves_channel(cli);

			} else if (cli->isAdmin) {

				if (find_other_clients(cli)) {
					// memset(buffer, '\0', strlen(buffer));
                    int clientFound = change_admin(cli);
                    
					if (clientFound) {
						client_leaves_channel(cli);
					} else {
						sprintf(buffer, "%sCliente não encontrado! Tente novamente...\n\n%s", serverMsgColor, defltColor);
						write(cli->sockfd, buffer, strlen(buffer));
					}
				} else {
					sprintf(buffer, "%sComo você era a única pessoa aqui, seu canal já era!%s\n\n", serverMsgColor, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
                    
                    delete_channel(cli);

					//client leaves the channel
					strcpy(cli->channel, "&default");
					cli->isAdmin = 0;

					channel_menu(cli);
				}
			}

		// Checks if the client wants to join some channel
		} else if(strncmp(msg, " /join", 6) == 0) {

			get_command(channel, msg, 7, CHANNEL_LEN);
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

							break;
						}

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
					sprintf(buffer, "%sInsira um nome de canal válido!\n\n%s", serverMsgColor, defltColor);
				}
				// If the user already participates in that channel
				else if(strcmp(cli->channel,channel) == 0){
					sprintf(buffer, "%sVocê já está neste canal!\n\n%s", serverMsgColor, defltColor);
				}
				// If it is an invite-only channel and the user has not been invited
				else if(!publicChannel && !invitedUser){
					sprintf(buffer, "%sDesculpe... Este é um canal invite-only e você não foi convidado.\n\n%s", serverMsgColor, defltColor);
				}
				// If there is already an user with that nickname on the channel
				else{
					sprintf(buffer, "%sJá existe um usuário com nickname %s nesse chat, para entrar mude seu nick com o comando: \"/nickname novo_nick\"!\n\n%s", serverMsgColor, cli->nick, defltColor);
				}

				write(cli->sockfd, buffer, strlen(buffer));

			// Dealing with the possibility of joining the channel
			} else {

				// Design decision: the user can only participate in one channel
				//at a time, so he cannot switch channels unless he disconnects
				//from the current one
				if (strcmp(cli->channel, channel_list[0].chName) != 0) {
					sprintf(buffer, "%sNada de ficar mudando de sala! Sem bagunça no KalinkUOL! Saia do servidor e entre novamente para poder se juntar a um outro canal.\n\n%s", serverMsgColor, defltColor);

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
							sprintf(buffer, "%sBem-vindo ao canal %s, vulgo melhor canal!\n\n%s",serverMsgColor, channel, defltColor);
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

									sprintf(buffer, "%sBem-vindo ao canal %s. Você é o admin! Lembre-se: com grandes poderes vêm grandes responsabilidades!\n\n%s",serverMsgColor, channel, defltColor);
									write(cli->sockfd, buffer, strlen(buffer));
									break;
								}
							}

						// If there's no room available...
						} else if (channelAvailable == 0) {
							memset(buffer, '\0', BUFFER_MAX);
							sprintf(buffer, "%sNão há espaço para novos canais!\n\n%s", serverMsgColor, defltColor);
							strcpy(cli->channel,channel_list[0].chName);
							write(cli->sockfd, buffer, strlen(buffer));
						}
					}

					//  Notifies other clients that this client has joined the channel
					sprintf(buffer, "%s%s entrou no canal %s!%s\n", cli->color, cli->nick, cli->channel, defltColor);
					printf("%s", buffer);

					send_message_to_channel(buffer, cli->userID, cli->channel, 0);
				}
			}
		} else if(strcmp(msg, " /ping\n") == 0) {

			char reply[5] = "pong\n";
			write(cli->sockfd, reply, strlen(reply));

		} else if(strncmp(msg, " /nickname", 10) == 0) {
			char oldName[NICK_LEN];
			memset(oldName, '\0', NICK_LEN);
			strcpy(oldName, cli->nick); 

			memset(nick, '\0', NICK_LEN);
			memset(cli->nick, '\0', NICK_LEN);

			// get new nickname
			get_command(nick, msg, 11, NICK_LEN);
		  	str_trim(nick, NICK_LEN);

			memset(buffer, '\0', BUFFER_MAX);
			sprintf(buffer, "\n%s%s agora se chama %s!\n\n%s", cli->color, oldName, nick, defltColor);
			printf("%s", buffer);
			send_message_to_channel(buffer, cli->userID, cli->channel, 0);

			//change the nickname
			strcpy(cli->nick, nick);

			memset(buffer, '\0', BUFFER_MAX);
			sprintf(buffer, "%sNick alterado para %s!\n\n%s", serverMsgColor, cli->nick, defltColor);
			write(cli->sockfd, buffer, strlen(buffer));

		} else if(strncmp(msg, " /kick", 6) == 0) {

			if(cli->isAdmin) {
				get_command(nick, msg, 7, NICK_LEN);
				str_trim(nick, NICK_LEN);

				int clientFound = find_client(nick,cli);

				if(clientFound!=-1){

					if(!clients[clientFound]->isAdmin){
							
							strcpy(clients[clientFound]->channel, channel_list[0].chName);

							memset(buffer, '\0', BUFFER_MAX);
							sprintf(buffer, "%sVocê foi eliminado do canal %s, talvez você devesse repensar suas ações.\n\n%s", serverMsgColor, cli->channel,defltColor);
							write(clients[clientFound]->sockfd, buffer, strlen(buffer));
							
							sleep(0.7);

							channel_menu(clients[clientFound]);

							sleep(0.7);

							memset(buffer, '\0', BUFFER_MAX);
							sprintf(buffer, "%s%s não está mais espalhando seu fedor no canal %s!\n\n%s", serverMsgColor, nick, cli->channel, defltColor);
							printf("%s", buffer);
							write(cli->sockfd, buffer, strlen(buffer));
						}
						else{
							memset(buffer, '\0', BUFFER_MAX);
							sprintf(buffer, "%sVocê não pode kikar a si mesmo do chat.\n\n%s", serverMsgColor, defltColor);
							write(clients[clientFound]->sockfd, buffer, strlen(buffer));
						}

				} else {
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sCliente %s não encontrado.\n\n%s", serverMsgColor, nick, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}

			} else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "%sTá achando que aqui é casa da mãe Joana?\nSe quer kickar geral, cria seu próprio canal!\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}

		} else if(strncmp(msg, " /mute", 6) == 0) {

			//only admin cans mute people
			if(cli->isAdmin) {

				//get who will be muted
				get_command(nick, msg, 7, NICK_LEN);
				str_trim(nick, NICK_LEN);

				int clientFound = find_client(nick,cli);

				if(clientFound!=-1){
					
						//notify that the client is muted
						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "%sShh, cala boquinha.\n\n%s", serverMsgColor, defltColor);
						write(clients[clientFound]->sockfd, buffer, strlen(buffer));

						clients[clientFound]->isMuted = 1;

						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "%s%s foi silenciadah!\n\n%s", serverMsgColor, nick, defltColor);
						printf("%s", buffer);
						write(cli->sockfd, buffer, strlen(buffer));
				}
				else {
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sCliente %s não encontrado.\n\n%s", serverMsgColor, nick, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}
			}
			else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "%sTá achando que aqui é casa da mãe Joana?\nSe quer mutar geral, cria seu próprio canal!\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}

		} else if(strncmp(msg, " /unmute", 8) == 0) {

			if(cli->isAdmin) {

				//get who will be unmuted
				get_command(nick, msg, 9, NICK_LEN);
				str_trim(nick, NICK_LEN);

				int clientFound = find_client(nick,cli);

				if(clientFound!=-1){
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sTá, pode falar.\n\n%s", serverMsgColor, defltColor);
					write(clients[clientFound]->sockfd, buffer, strlen(buffer));

					clients[clientFound]->isMuted = 0;

					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%s%s foi liberadah!\n\n%s", serverMsgColor, nick, defltColor);
					printf("%s", buffer);
					write(cli->sockfd, buffer, strlen(buffer));

				} else {
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sCliente %s não encontrado.\n\n%s", serverMsgColor, nick, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}

			} else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "%sTá achando que aqui é casa da mãe Joana?\nPode sair desmutando assim não!\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}

		} else if(strncmp(msg, " /whois", 7) == 0) {

			if(cli->isAdmin) {
				get_command(nick, msg, 8, NICK_LEN);
				str_trim(nick, NICK_LEN);

				int clientFound = find_client(nick,cli);

				if(clientFound!=-1){

					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sO endereço de IP de %s é %s\n\n%s", serverMsgColor, clients[clientFound]->nick,inet_ntoa(clients[clientFound]->address.sin_addr), defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
					
				} else {

					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sCliente %s não encontrado.\n\n%s", serverMsgColor, nick, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}

			}else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "%sTá achando que aqui é casa da mãe Joana?\nPode sair querendo saber os IP dos outros assim não!\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}


		} else if(strncmp(msg, " /mode", 6) == 0) {

			if(cli->isAdmin) {

				get_command(mode, msg, 7, 3);
				str_trim(mode, 3);

				// Finding the channel for which the administrator is responsible
				int idChannel = find_channel(cli);

				if(strcmp(mode, "+i") == 0 && strcmp(channel_list[idChannel].chMode, "-i") == 0){
					strcpy(channel_list[idChannel].chMode, mode);

					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sEste canal agora é invite-only!\n\n%s", serverMsgColor, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}
				else if(strcmp(mode, "+i") == 0 && strcmp(channel_list[idChannel].chMode, "+i") == 0){
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sEste canal já é invite-only!\n\n%s", serverMsgColor, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}
				else if (strcmp(mode, "-i") == 0 && strcmp(channel_list[idChannel].chMode, "+i") == 0) {
					strcpy(channel_list[idChannel].chMode, mode);

					clear_invite_list(idChannel);

					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sEste canal não é mais invite-only, qualquer um pode entrar!\n\n%s", serverMsgColor, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}
				else if(strcmp(mode, "-i") == 0 && strcmp(channel_list[idChannel].chMode, "-i") == 0){
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sEste canal já é aberto!\n\n%s", serverMsgColor, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}
				else {
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sModo inválido, únicas opções +i ou -i !\n\n%s", serverMsgColor, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));
				}
		
			}else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "%sPoxa... Somente o administrador possui o direito de mudar o mode do canal.\n\n%s", serverMsgColor, defltColor);
				write(cli->sockfd, buffer, strlen(buffer));
			}


		} else if(strncmp(msg, " /invite", 8) == 0) {

			if(cli->isAdmin) {

				//get who will be invited
				get_command(nick, msg, 9, NICK_LEN);
				str_trim(nick, NICK_LEN);

				int clientFound = 0;
				int clientExists = 0;
				int idClient = -1;

				// Finding the channel for which the administrator is responsible
				int idChannel = find_channel(cli);

				if(strcmp(channel_list[idChannel].chMode,"+i")!=0){
					memset(buffer, '\0', BUFFER_MAX);
					sprintf(buffer, "%sNão é possível convidar alguém para um canal que não é invite-only.\n\n%s", serverMsgColor, defltColor);
					write(cli->sockfd, buffer, strlen(buffer));

				} else {
					// Checking if the user exists and if isn't already invited to that channel
					for (int i = 0; i < MAX_CLI; i++) {

						if (clients[i] && strcmp(nick, clients[i]->nick) == 0){
							clientExists = 1;
							idClient = i;
						}
						if (strcmp(channel_list[idChannel].inviteUser[i], nick) == 0) {
							memset(buffer, '\0', BUFFER_MAX);
							sprintf(buffer, "%sO usuário %s já foi convidado a se juntar a este chat.\n\n%s", serverMsgColor, nick, defltColor);
							write(cli->sockfd, buffer, strlen(buffer));
							clientFound = 1;
						}
						if(clientExists && clientFound)
							break;
							
					}

					// If the user has not yet been invited and it is possible to
					//invite more users to the chat, the process is done
					if(clientExists && !clientFound && channel_list[idChannel].nroInvUser < MAX_CLI - 1){
						for(int i = 0; i < MAX_CLI; i++){
							if(channel_list[idChannel].inviteUser[i][0] == '\0'){
								strcpy(channel_list[idChannel].inviteUser[i], nick);

								memset(buffer, '\0', BUFFER_MAX);
								sprintf(buffer, "%sO usuário %s foi convidado a se juntar a este chat.\n\n%s", serverMsgColor, nick, defltColor);
								write(cli->sockfd, buffer, strlen(buffer));
							
								memset(buffer, '\0', BUFFER_MAX);
								sprintf(buffer, "%sVocê recebeu um free pass para o canal %s, para poucos viu.\n\n%s", serverMsgColor,cli->channel, defltColor);
								write(clients[idClient]->sockfd, buffer, strlen(buffer));

								channel_list[idChannel].nroInvUser++;

								break;
							}
						}

					}
					else if(!clientExists){
						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "%sO usuário precisa estar conectado ao servidor para poder ser convidado a participar deste canal.%s\n\n", serverMsgColor, defltColor);
						write(cli->sockfd, buffer, strlen(buffer));
					}
					else if(channel_list[idChannel].nroInvUser >= MAX_CLI - 1){
						memset(buffer, '\0', BUFFER_MAX);
						sprintf(buffer, "%sO canal já atingiu o número máximo de usuários convidados.%s\n\n", serverMsgColor, defltColor);
						write(cli->sockfd, buffer, strlen(buffer));
					}
				}
			}
			else {
				memset(buffer, '\0', BUFFER_MAX);
				sprintf(buffer, "%sPoxa... Somente o administrador pode convidar usuários para este canal.\n\n%s", serverMsgColor, defltColor);
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
			printf("\nErro, conexão prejudicada.\n");
			leaveFlag = 1;
		}

		memset(buffer, '\0', BUFFER_MAX);
		memset(msg, '\0', MSG_LEN);

		sleep(0.7);
	}

	// When client leaves the chat
	close(cli->sockfd);
	remove_client(cli->userID/*, clients, clients_mutex*/);
	cliCount--;
	free(cli);
	// Marks the thread identified by thread as detached
	pthread_detach(pthread_self());

	return NULL;
}