all:
	gcc -Wall -g -pthread string_manipulation.c server_operation.c server.c -o server
	gcc -Wall -g -pthread client.c -o client

server: 
	gcc -Wall -g -pthread string_manipulation.c server_operation.c server.c -o server

client:
	gcc -Wall -g -pthread client.c -o client

run_server: 
	./server || true

run_client:
	./client 127.0.0.1 || true