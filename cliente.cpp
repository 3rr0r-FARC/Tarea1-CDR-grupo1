#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <IP> <port>" << std::endl;
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Could not create socket" << std::endl;
        return 1;
    }

    sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect failed. Error");
        return 1;
    }

    std::cout << "Connected to server" << std::endl;
    char buffer[1024];

    // Recibir y mostrar el tablero inicial
    int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived > 0) {
        std::cout << "Tablero inicial:\n" << std::string(buffer, bytesReceived) << std::endl;
    }

    std::string input;
    while (true) {
        std::cout << "Enter column (0-6) or 'exit' to quit: ";
        std::cin >> input;
        if (input == "exit") break;

        if (send(sock, input.c_str(), input.length(), 0) < 0) {
            std::cerr << "Send failed" << std::endl;
            return 1;
        }

        // Recibir y mostrar el tablero actualizado
        bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            std::cout << std::string(buffer, bytesReceived) << std::endl;
        } else {
            std::cerr << "Error receiving data" << std::endl;
            break;
        }
    }

    close(sock);
    return 0;
}
