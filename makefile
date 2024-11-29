client: chat_client.c
	gcc -o client -std=gnu99 -pthread -Wall chat_client.c

server: chat_server.c
	gcc -o server -std=gnu99 -pthread -Wall chat_server.c
