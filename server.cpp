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

const int PORT = 7777;
const int MAX_CLIENTS = 10;
const int BOARD_ROWS = 6;
const int BOARD_COLS = 7;
const char PLAYER_ONE = 'X';
const char PLAYER_TWO = 'O';
const char EMPTY_SPACE = '.';

std::mutex games_mutex;

struct Game {
    char board[BOARD_ROWS][BOARD_COLS];
    int current_player;
    bool game_over;

    Game() : current_player(0), game_over(false) {
        for (int i = 0; i < BOARD_ROWS; ++i) {
            for (int j = 0; j < BOARD_COLS; ++j) {
                board[i][j] = EMPTY_SPACE;
            }
        }
    }

    // Función para realizar un movimiento
    bool makeMove(int column) {
        if (column < 0 || column >= BOARD_COLS) {
            return false; // Columna inválida
        }
        // Encuentra la fila más baja disponible en esa columna
        for (int i = BOARD_ROWS - 1; i >= 0; --i) {
            if (board[i][column] == EMPTY_SPACE) {
                board[i][column] = current_player == 0 ? PLAYER_ONE : PLAYER_TWO;
                checkWin(i, column);
                current_player = 1 - current_player; // Cambiar de jugador
                return true;
            }
        }
        return false; // Columna llena
    }

    bool checkLine(const char token, const int start_row, const int start_col, const int delta_row, const int delta_col) {
    int count = 0;
    // Verificar en una dirección
    for (int i = 0; i < 4; ++i) {
        int r = start_row + i * delta_row;
        int c = start_col + i * delta_col;
        if (r < 0 || r >= BOARD_ROWS || c < 0 || c >= BOARD_COLS || board[r][c] != token) {
            return false;
        }
        count++;
    }
    return count == 4;
}

void checkWin(const int last_row, const int last_col) {
    const char player_token = board[last_row][last_col];
    // Verificar todas las direcciones desde la posición actual
    const int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}}; // horizontal, vertical, diagonal1, diagonal2

    for (auto& dir : directions) {
        if ((checkLine(player_token, last_row, last_col, dir[0], dir[1])) ||
            (checkLine(player_token, last_row, last_col, -dir[0], -dir[1]))) {
            game_over = true;
            return;
        }
    }
}


    // Función para reiniciar el juego
    void resetGame() {
        for (int i = 0; i < BOARD_ROWS; ++i) {
            for (int j = 0; j < BOARD_COLS; ++j) {
                board[i][j] = EMPTY_SPACE;
            }
        }
        current_player = 0;
        game_over = false;
    }
};

std::string serializeBoard(const Game& game) {
    std::string boardStr;
    for (int i = 0; i < BOARD_ROWS; ++i) {
        for (int j = 0; j < BOARD_COLS; ++j) {
            boardStr += game.board[i][j];
            boardStr += " ";  // Espacio para separar columnas
        }
        boardStr += "\n"; // Nueva línea al final de cada fila
    }
    return boardStr;
}

std::vector<Game> games;

struct ClientData {
    int sock;
    struct sockaddr_in address;
    Game* game;
};

void* handle_client(void* arg) {
    ClientData* data = static_cast<ClientData*>(arg);
    int sock = data->sock;
    struct sockaddr_in clientAddr = data->address;
    Game* game = data->game;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(clientAddr.sin_port);

    std::cout << "Nuevo jugador conectado desde [" << client_ip << ":" << client_port << "]." << std::endl;

    char buffer[1024];

    // Envía el tablero inicial al conectar
    std::string boardState = serializeBoard(*game);
    std::string response = "Ingrese columna (0-6) o 'exit' para salir: \n";
    send(sock, response.c_str(), response.length(), 0);
    send(sock, boardState.c_str(), boardState.length(), 0);


    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int n_bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n_bytes <= 0) {
            std::cout << "[" << client_ip << ":" << client_port << "] El jugador se ha desconectado." << std::endl;
            break; // Salir si el cliente se desconecta
        }

        buffer[n_bytes] = '\0'; // Asegurar que el buffer es una cadena válida

        std::cout << "[" << client_ip << ":" << client_port << "] Recibido: " << buffer << std::endl;

        buffer[strcspn(buffer, "\n")] = 0;
        buffer[strcspn(buffer, "\r")] = 0;

        if (std::string(buffer) == "exit") {
            std::cout << "[" << client_ip << ":" << client_port << "] El jugador abandona el juego." << std::endl;
            break;
        }

        std::string input(buffer);
        std::istringstream iss(input);
        int column;
        char extraChar;

        if (!(iss >> column) || iss >> extraChar) {
            // Si no se pudo leer un entero o si hay caracteres extras después del número
            std::string response = "Movimiento inválido\n" + serializeBoard(*game);
            send(sock, response.c_str(), response.length(), 0);
        } else {
            // Procesa el movimiento
            if (game->makeMove(column)) {
                if (game->game_over) {
                    std::string response = "¡Ganaste!\n" + serializeBoard(*game);
                    send(sock, response.c_str(), response.length(), 0);
                    game->resetGame();  // Opcional: reiniciar el juego automáticamente
                } else {
                    std::string response = "Movimiento aceptado\n" + serializeBoard(*game);
                    send(sock, response.c_str(), response.length(), 0);
                }
            } else {
                std::string response = "Movimiento inválido\n" + serializeBoard(*game);
                send(sock, response.c_str(), response.length(), 0);
            }
        }
    }

    close(sock);
    delete data;
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "Uso: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = atoi(argv[1]);
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error de enlace");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Escucha fallida");
        exit(EXIT_FAILURE);
    }

    std::cout << "El servidor está escuchando en el puerto " << port << std::endl;

    while (true) {
        int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("Aceptar fallo");
            continue;
        }

        Game* newGame = new Game();
        ClientData* clientData = new ClientData{client_sock, client_addr, newGame};

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, (void*)clientData) != 0) {
            std::cerr << "Error al crear hilo" << std::endl;
            delete newGame;
            delete clientData;
            close(client_sock);
        }
        pthread_detach(thread);
    }

    return 0;
}
