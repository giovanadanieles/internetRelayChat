// === FUNCTIONS RELATED TO STRING MANIPULATION ===
#include <stdio.h>
#include <string.h>

#define NICK_LEN 50

// Responsible for overwriting and flushing the stdout.
void str_overwrite_stdout();

/* Responsible for removing any undesirable '\n'

	PARAMETERS
	char* arr - any input string
	int   len - size of input strings */
void str_trim(char* arr, int len);

/* Separates nick and message from incoming buffer.
	
	PARAMETERS
	char* buffer - incoming buffer
	char* msg 	 - incoming message (without user nickname) */
void nick_trim(char* buffer, char* msg);

/* Changes nickname color.

	PARAMETERS
	char* buffer - incoming buffer 
	char* n 	 - user's nickame */
void change_color(char* buffer, char* n);

/* Gets command from user input.

	PARAMETERS
	char* sub - command
	char* msg - message sent by user
	int commandLen - command length
	int maxLen 	   - maximum length */
void get_command(char* sub, char* msg, int commandLen, int maxLen);