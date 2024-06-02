CC = g++
CFLAGS = -pthread -std=c++11
CLIENT_SRC = cliente.cpp
CLIENT_TARGET = cliente

all: $(CLIENT_TARGET)

$(CLIENT_TARGET): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) $(CLIENT_SRC)

clean:
	rm -f $(CLIENT_TARGET)