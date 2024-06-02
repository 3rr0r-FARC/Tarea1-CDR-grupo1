CC = g++
CFLAGS = -pthread -std=c++11
SERVER_SRC = server.cpp
SERVER_TARGET = server

all: $(SERVER_TARGET)

$(SERVER_TARGET): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_TARGET) $(SERVER_SRC)

clean:
	rm -f $(SERVER_TARGET) 
