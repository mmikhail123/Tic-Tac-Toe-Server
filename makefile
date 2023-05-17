CC     = gcc
CFLAGS = -std=c99 -g -Wall -fsanitize=address,undefined

all: ttt ttts 

ttt: ttt.c protocol.c protocol.h
	$(CC) $(CFLAGS) ttt.c protocol.c protocol.h -o ttt

ttts: ttts.c protocol.c protocol.h
	$(CC) $(CFLAGS) -pthread ttts.c protocol.c protocol.h -o ttts 

test-invalid-client: test-invalid-client.c protocol.c protocol.h
	$(CC) $(CFLAGS) test-invalid-client.c protocol.c protocol.h -o test-invalid-client

test-game-client: test-game-client.c protocol.c protocol.h
	$(CC) $(CFLAGS) test-game-client.c protocol.c protocol.h -o test-game-client

clean:
	rm -rf *.o all
