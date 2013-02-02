VPATH=src
FLAGS=-std=c11 -lrt
CC=clang

server: server.c
	$(CC) $(FLAGS) -o bin/$@ $^

clean:
	rm bin/*
