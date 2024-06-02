#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <random>

const int MAX_CLIENTS = 10;
const int BOARD_ROWS = 6;
const int BOARD_COLS = 7;
const char PLAYER_ONE = 'C';
const char PLAYER_TWO = 'S';
const char EMPTY_SPACE = '.';

std::mutex games_mutex;

class Game
{
private:
    char board[BOARD_ROWS][BOARD_COLS];
    int current_player;
    int starter;
    bool game_over;

public:
    Game()
        : current_player(0), game_over(false)
    {
        resetGame();
    }

    bool makeMove(int column)
    {
        column -= 1; // Ajustar la columna para que sea de 1 a 7
        if (column < 0 || column >= BOARD_COLS)
        {
            return false; // Columna inválida
        }
        // Encuentra la fila más baja disponible en esa columna
        for (int i = BOARD_ROWS - 1; i >= 0; --i)
        {
            if (board[i][column] == EMPTY_SPACE)
            {
                board[i][column] = current_player == 0 ? PLAYER_ONE : PLAYER_TWO;
                checkWin(i, column);
                current_player = 1 - current_player; // Cambiar de jugador
                return true;
            }
        }
        return false; // Columna llena
    }

    std::string serializeBoard() const
    {
        std::ostringstream boardStr;
        boardStr << "TABLERO\n";
        for (int i = 0; i < BOARD_ROWS; ++i)
        {
            boardStr << (i + 1) << " ";
            for (int j = 0; j < BOARD_COLS; ++j)
            {
                boardStr << board[i][j] << " ";
            }
            boardStr << "\n";
        }
        boardStr << "-------------------------\n";
        boardStr << "  ";
        for (int j = 0; j < BOARD_COLS; ++j)
        {
            boardStr << (j + 1) << " ";
        }
        boardStr << "\n";
        return boardStr.str();
    }

    bool isGameOver() const
    {
        return game_over;
    }

    int getCurrentPlayer() const
    {
        return current_player;
    }

    int getStarter() const
    {
        return starter;
    }

    void computerMove(std::string client_ip, int client_port)
    {
        std::vector<int> availableColumns;
        for (int col = 1; col <= BOARD_COLS; ++col) 
        {
            for (int row = BOARD_ROWS - 1; row >= 0; --row)
            {
                if (board[row][col - 1] == EMPTY_SPACE) // Ajustar el índice de la columna
                {
                    availableColumns.push_back(col);
                    break; // Solo necesitamos saber que al menos una fila en esta columna está libre.
                }
            }
        }

        if (!availableColumns.empty())
        {
            std::random_device rd;  // Obtener un número aleatorio del dispositivo de hardware
            std::mt19937 gen(rd()); // Motor de generación de números aleatorios
            std::uniform_int_distribution<> distrib(0, availableColumns.size() - 1);

            int column = availableColumns[distrib(gen)]; // Elije una columna al azar
            // Realiza el movimiento en la primera fila disponible desde abajo hacia arriba en la columna elegida.
            makeMove(column); // Realiza el movimiento
            std::cout << "Juego [" << client_ip << ":" << client_port << "]: servidor juega columna " << column << "." << std::endl;

        }
    }

    void resetGame()
    {
        for (int i = 0; i < BOARD_ROWS; ++i)
        {
            for (int j = 0; j < BOARD_COLS; ++j)
            {
                board[i][j] = EMPTY_SPACE;
            }
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 1);
        starter = dist(gen); // 0 para el jugador, 1 para la computadora
        current_player = starter;
        game_over = false;
    }

private:
    bool checkLine(const char token, const int start_row, const int start_col, const int delta_row, const int delta_col)
    {
        int count = 0;
        // Verificar en una dirección
        for (int i = 0; i < 4; ++i)
        {
            int r = start_row + i * delta_row;
            int c = start_col + i * delta_col;
            if (r < 0 || r >= BOARD_ROWS || c < 0 || c >= BOARD_COLS || board[r][c] != token)
            {
                return false;
            }
            count++;
        }
        return count == 4;
    }

    void checkWin(const int last_row, const int last_col)
    {
        const char player_token = board[last_row][last_col];
        // Verificar todas las direcciones desde la posición actual
        const int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}}; // horizontal, vertical, diagonal1, diagonal2

        for (auto &dir : directions)
        {
            if ((checkLine(player_token, last_row, last_col, dir[0], dir[1])) ||
                (checkLine(player_token, last_row, last_col, -dir[0], -dir[1])))
            {
                game_over = true;
                return;
            }
        }
    }
};

class Server
{
private:
    int server_fd;
    struct sockaddr_in server_addr;
    std::vector<std::thread> threads;
    std::vector<Game> games;

public:
    Server(int port)
    {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0)
        {
            perror("No se pudo crear el socket");
            exit(EXIT_FAILURE);
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            perror("Error de enlace");
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, MAX_CLIENTS) < 0)
        {
            perror("Escucha fallida");
            exit(EXIT_FAILURE);
        }

        std::cout << "Esperando conexiones ..." << std::endl;
    }

    ~Server()
    {
        close(server_fd);
        for (auto &t : threads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
    }

    void run()
    {
        while (true)
        {
            struct sockaddr_in client_addr;
            socklen_t addr_size = sizeof(client_addr);
            int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
            if (client_sock < 0)
            {
                perror("Aceptar fallo");
                continue;
            }

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);
            std::cout << "Juego nuevo [" << client_ip << ":" << client_port << "]" << std::endl;

            std::unique_lock<std::mutex> lock(games_mutex);
            games.emplace_back();
            Game &newGame = games.back();
            lock.unlock();

            threads.emplace_back(&Server::handleClient, this, client_sock, client_addr, &newGame, std::string(client_ip), client_port);
        }
    }

private:
    void handleClient(int client_sock, struct sockaddr_in client_addr, Game *game, std::string client_ip, int client_port)
    {
        char buffer[1024];

        while (true)
        {
            std::string startMessage = (game->getStarter() == 0) ? "El jugador comienza el juego.\n" : "El servidor comienza el juego.\n";
            std::cout << "Juego [" << client_ip << ":" << client_port << "]: " << startMessage;
            send(client_sock, startMessage.c_str(), startMessage.length(), 0);

            if (game->getStarter() == 1)
            {
                game->computerMove(client_ip, client_port);
            }

            // Envía el tablero inicial al conectar o después de reiniciar
            std::string boardState = game->serializeBoard();
            send(client_sock, boardState.c_str(), boardState.length(), 0);

            while (true)
            {
                memset(buffer, 0, sizeof(buffer));
                int n_bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                if (n_bytes <= 0)
                {
                    std::cout << "[" << client_ip << ":" << client_port << "] El jugador se ha desconectado." << std::endl;
                    close(client_sock);
                    return; // Salir si el cliente se desconecta
                }

                buffer[n_bytes] = '\0'; // Asegurar que el buffer es una cadena válida

                std::cout << "Juego [" << client_ip << ":" << client_port << "]: Recibido: " << buffer << std::endl;

                buffer[strcspn(buffer, "\n")] = 0;
                buffer[strcspn(buffer, "\r")] = 0;

                if (std::string(buffer) == "exit")
                {
                    std::cout << "Juego [" << client_ip << ":" << client_port << "]: El jugador abandona el juego." << std::endl;
                    close(client_sock);
                    return;
                }

                if (std::string(buffer) == "r")
                {
                    game->resetGame();
                    if (game->getStarter() == 1)
                    {
                        game->computerMove(client_ip, client_port);
                    }
                    break; // Salir del bucle interno para reiniciar el juego
                }

                std::string input(buffer);
                std::istringstream iss(input);
                int column;
                char extraChar;

                if (!(iss >> column) || iss >> extraChar)
                {
                    // Si no se pudo leer un entero o si hay caracteres extras después del número
                    std::string response = "Movimiento inválido\n" + game->serializeBoard();
                    send(client_sock, response.c_str(), response.length(), 0);
                }
                else if (game->makeMove(column))
                {
                    std::cout << "Juego [" << client_ip << ":" << client_port << "]: cliente juega columna " << column << "." << std::endl;
                    if (game->isGameOver())
                    {
                        std::cout << "Juego [" << client_ip << ":" << client_port << "]: El jugador gana!\n" << std::endl;
                        std::string response = "¡Ganaste!\n" + game->serializeBoard();
                        send(client_sock, response.c_str(), response.length(), 0);
                    }
                    else
                    {
                        game->computerMove(client_ip, client_port); // Llama a la función de movimiento del servidor
                        std::string response = "Movimiento aceptado\n" + game->serializeBoard();
                        send(client_sock, response.c_str(), response.length(), 0);
                        if (game->isGameOver())
                        {
                            std::cout << "Juego [" << client_ip << ":" << client_port << "]: El servidor gana!\n" << std::endl;
                            response = "El servidor gana!\n" + game->serializeBoard();
                            send(client_sock, response.c_str(), response.length(), 0);
                        }
                    }
                }
                else
                {
                    std::string response = "Movimiento inválido\n" + game->serializeBoard();
                    send(client_sock, response.c_str(), response.length(), 0);
                }
            }
        }
    }
};

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Uso: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);

    Server server(port);
    server.run();

    return 0;
}
