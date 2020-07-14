#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
// #include <string.h>
// #include <netinet/in.h>
// #include <sys/socket.h>
// #include <sys/types.h>

#include "string_manipulation.h"

#define BUFFER_MAX 4097
#define MAX_CLI 10
#define MSG_LEN 2049
#define CHANNEL_LEN 200
#define CHANNEL_NUM 10

// === STRUCTURES RELATED TO SERVER OPERATION ===

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

/* Channels names are strings (beginning with a '&' or '#' character) of
length up to 200 characters.  Apart from the the requirement that the
first character being either '&' or '#'; the only restriction on a
channel name is that it may not contain any spaces (' '), a control G 
(^G or ASCII 7), or a comma (',' which is used as a list item
separator by the protocol). */

typedef struct {
	char chName[CHANNEL_LEN];
	char chMode[3];
	char inviteUser[MAX_CLI][NICK_LEN];
	int nroInvUser;
} Channel;

// === FUNCTIONS RELATED TO SERVER OPERATION ===

void is_server_full(int connfd);

/* Adds clients to the array of clients.

	PARAMETERS
	Client* cli - client to be added */
void add_client(Client* cli/*, Client** clients, pthread_mutex_t clients_mutex, char** usrColors*/);

/* Creates client structure, defines client settings, adds them to the queue,
creates a thread and a new function to handle client.

	PARAMETERS
	struct sockaddr_in client_addr - client's address
	int connfd - socket file descriptor
	int userID - current user ID

	RETURN
	Client* - new client */
void create_client(struct sockaddr_in client_addr, int connfd, Client* cli);

/* Removes clients from the array of clients.

	PARAMETERS
	int userID - current user ID */
void remove_client(int userID);

/* Sends messages to all the clients, except the sender itself.

	PARAMETERS
	char* msg 	  	- message to be sent
	int   userID  	- current user ID 
	char* channel 	- current user's channel
	int   leaveFlag - current user's leave flag */
void send_message_to_channel(char* msg, int userID, char* channel, int leaveFlag);

/* Checks whether the channel name is valid. 
	
	PARAMETERS
	char* channel - channel name */
int check_channel(char* channel);

/* Checks if there is already a user with the specified nickname on the specified channel. 

	PARAMETERS
	char* nick 	  - user nickname
	char* channel - channel name */
int check_nick(char* nick, char* channel);

/* Creates initial channel list.

	PARAMETERS
	Channel* channel_list - list of exisiting channels */
void initialize_channel_list();

/* Shows channel menu.

	PARAMETERS
	Client* cli - current client */
void channel_menu(Client* cli);

/* Shows welcome menu.

	PARAMETERS
	Client* cli - current client */
void welcome_menu(Client* cli);


/* Handles client leaving channel.

	PARAMETERS
	Client* cli - current client */
void client_leaves_channel(Client* cli);

/* Find other clients in the same channel and list.

	PARAMETERS
	Client* cli - current client */
int find_other_clients(Client* cli);

/* Deletes existing channel.

	PARAMETERS
	Client* cli - current client */
void delete_channel(Client* cli);

/* Changes channel's admin.

	PARAMETERS
	Client* cli - current client*/
int change_admin(Client*cli);

/* Finds clients in the same channel.

	PARAMETERS
	char* nick  - nickname of user to be searched for
	Client* cli - current client */
int find_client(char* nick, Client* cli);

/* Finds current client's channel.

	PARAMETERS
	Client* cli - current client */
int find_channel(Client* cli);

/* Handles clients, assigns their values and joins the chat.

	PARAMETERS
	void* arg - client structure */
void* handle_client(void* arg);