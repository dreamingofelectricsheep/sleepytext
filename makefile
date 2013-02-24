VPATH=src:bin
FLAGS=-std=gnu99
CC=clang
SQLITE_FLAGS=-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION

server: server.c sqlite.o
	$(CC) $(FLAGS) -lrt -lssl -lcrypto -o bin/$@ $^


sqlite.o: sqlite3.c
	$(CC) -c $(SQLITE_FLAGS) -o bin/$@ $^

clean:
	rm bin/*
