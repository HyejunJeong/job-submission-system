CFLAGS = -g -O2 -Wall -Werror -Wextra

SRC_SERVER=$(wildcard src/server/*.c)
SRC_CLIENT=$(wildcard src/client/*.c)

all: Server Client
	chmod +x ./Server
	chmod +x ./Client

Server: $(SRC_SERVER)
	gcc -o $@ $^ $(CFLAGS)

Client: $(SRC_CLIENT)
	gcc -o $@ $^ $(CFLAGS)


.PHONY: clean
clean:
	rm -rf ./Server ./Client