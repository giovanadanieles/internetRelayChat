// === FUNCTIONS RELATED TO STRING MANIPULATION ===
#include "string_manipulation.h"

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

// Separates nick and message from incoming buffer
void nick_trim(char* buffer, char* msg) {
	for(int j = 0; j < NICK_LEN; j++) {
		if(buffer[j] == ':') {
			strcpy(msg, buffer+j+1);
			break;
		}
	}
}

// Changes nickname color
void change_color(char *buffer, char *n) {
	int k;

	memset(n, '\0', NICK_LEN);
	for(k = 0; k < NICK_LEN; k++) {
		if(buffer[k] == ':') break;
		else n[k] = buffer[k];
	}
}

// Gets command from user input.
void get_command(char* sub, char* msg, int commandLen, int maxLen) {
	for (int i = commandLen; i <= commandLen+maxLen; i++) {
		sub[i-commandLen] = msg[i];
	}
}