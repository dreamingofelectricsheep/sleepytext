VPATH=src:bin
flags=-std=gnu99
cc=clang
sqlite3_flags=-DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION

all: server main.html
	

objs=server.o sqlite3.o
server: $(objs)
	$(cc) $(flags) -lrt -lssl -lcrypto -o bin/$@ $(patsubst %,bin/%,$(objs))

-include $(patsubst %.o,bin/%.d,$(objs))

%.o: %.c
	$(cc) -c $(flags) $($*_flags) -o bin/$*.o src/$*.c
	$(cc) -MM $(flags) src/$*.c > bin/$*.d

main.html:
	./html/build.sh

clean:
	rm bin/*
