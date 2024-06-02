#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>

std::mutex game_over_mutex;
bool game_over = false;

void handleServerResponse(int sock)
{
    char buffer[1024];

    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int n_bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n_bytes <= 0)
        {
            std::cout << "Conexión cerrada por el servidor." << std::endl;
            break;
        }
        buffer[n_bytes] = '\0';
        std::cout << buffer << std::endl;

        if (std::string(buffer).find("¡Ganaste!") != std::string::npos || std::string(buffer).find("El servidor gana!") != std::string::npos)
        {
            std::lock_guard<std::mutex> lock(game_over_mutex);
            game_over = true;
            std::cout << "Ingrese 'r' para volver a jugar o 'exit' para salir.\n";
        }
    }
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cout << "Uso: " << argv[0] << " <direccion_servidor> <puerto>" << std::endl;
        return 1;
    }

    const char *server_ip = argv[1];
    int port = std::stoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("No se pudo crear el socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        perror("Dirección IP inválida");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Conexión fallida");
        close(sock);
        return 1;
    }

    std::cout << "Conectado al servidor en " << server_ip << ":" << port << std::endl;

    std::thread responseThread(handleServerResponse, sock);

    while (true)
    {
        std::string input;
        std::cout << "Ingrese columna (1-7) o 'exit' para salir: \n";
        std::getline(std::cin, input);

        // Bloquear cualquier ingreso que no sea 'r' o 'exit' cuando el juego haya terminado
        while (true)
        {
            std::lock_guard<std::mutex> lock(game_over_mutex);
            if (!game_over || (input == "r" || input == "exit"))
            {
                break;
            }
            std::cout << "Entrada inválida. Ingrese 'r' para volver a jugar o 'exit' para salir: ";
            std::getline(std::cin, input);
        }

        if (send(sock, input.c_str(), input.length(), 0) < 0)
        {
            perror("Error al enviar datos");
            break;
        }

        if (input == "exit")
        {
            break;
        }

        // Resetear el estado de game_over si se reinicia el juego
        if (input == "r")
        {
            std::lock_guard<std::mutex> lock(game_over_mutex);
            game_over = false;
        }
    }

    responseThread.join();
    close(sock);

    return 0;
}
