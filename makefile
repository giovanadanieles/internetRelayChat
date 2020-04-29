all:
	gcc -pthread server.c -o server -g -Wall
	gcc -pthread client.c -o client -g -Wall

server: 
	gcc -pthread server.c -o server -g -Wall

client:
	gcc -pthread client.c -o client -g -Wall

run_server: 
	./server

run_client:
	./client