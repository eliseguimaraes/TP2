IP = 127.0.0.1
PORT = 32000
FILE = arquivo.txt
BUFFER = 8
JANELA = 3
CC = gcc

server:
	$(CC) client.c -lm -o client

run:
	./client $(IP) $(PORT) $(FILE) $(BUFFER) $(JANELA)
