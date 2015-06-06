IP = 127.0.0.1
PORT = 32000
FILE = rfc-index.txt
BUFFER = 100
JANELA = 10
CC = gcc

server:
	$(CC) client.c -lm -o client

run:
	./client $(IP) $(PORT) $(FILE) $(BUFFER) $(JANELA)
