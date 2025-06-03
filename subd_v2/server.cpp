#include "subd.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <exception>
#include <thread>
#include <mutex>
#include <set>
#include <unordered_map>

// Класс для временного перенаправления потока
class coutRedirect {
    std::streambuf* old_buf;
    std::stringstream buffer;
    
public:
    coutRedirect() : old_buf(std::cout.rdbuf()) {
        std::cout.rdbuf(buffer.rdbuf());
    }
    
    ~coutRedirect() {
        std::cout.rdbuf(old_buf);
    }
    
    std::string getOutput() const {
        return buffer.str();
    }
};

// Парсинг конфига
std::map<std::string, std::string> read_config(const std::string& filename) {
    std::ifstream file(filename);
    std::map<std::string, std::string> config;
    std::string line;
    
    while (std::getline(file, line)) {
        size_t delim_pos = line.find('=');
        if (delim_pos != std::string::npos) {
            std::string key = line.substr(0, line.find_last_not_of(" \t", delim_pos - 1) + 1);
            std::string value = line.substr(delim_pos + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            config[key] = value;
        }
    }
    
    return config;
}

std::mutex db_map_mutex;
std::set<int> client_sockets;
std::mutex clients_mutex;
std::unordered_map<std::string, std::set<int>> file_clients_map;

// Для каждого файла: массив экземпляров Database
std::unordered_map<std::string, std::vector<std::shared_ptr<Database>>> db_instances_map;

void notify_clients_db_update(const std::string& filename) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = file_clients_map.find(filename);
    if (it != file_clients_map.end()) {
        for (int sock : it->second) {
            int update_code = -1; // специальный код для "БД обновлена"
            send(sock, &update_code, sizeof(int), 0);
        }
    }
}

void handle_client(int clientSocket) {
    std::string current_db_file;
    std::shared_ptr<Database> db_ptr;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        client_sockets.insert(clientSocket);
    }
    while (true) {
        int msgLength;
        ssize_t bytesRead = recv(clientSocket, &msgLength, sizeof(int), 0);
        if (bytesRead <= 0) {
            std::cerr << "\033[1;31mКлиент отключился или произошла ошибка\033[0m\n";
            break;
        }
        if (msgLength == -1) {
            // Это уведомление от сервера, клиенту не нужно отвечать
            continue;
        }
        try {
            if (msgLength <= 0) {
                std::cerr << "\033[1;31mНекорректная длина сообщения\033[0m\n";
                close(clientSocket);
                return;
            }
            std::vector<char> buffer(msgLength + 1);
            int totalReceived = 0;
            while (totalReceived != msgLength) {
                bytesRead = recv(clientSocket, buffer.data() + totalReceived, msgLength - totalReceived, 0);
                if (bytesRead <= 0) {
                    std::cerr << "\033[1;31mОшибка получения ответа\033[0m\n";
                    close(clientSocket);
                    return;
                }
                totalReceived += bytesRead;
            }
            buffer[msgLength] = '\0';

            std::string wmessage = buffer.data();
            std::cout << "Получено от клиента: " << wmessage << std::endl;
            std::string captured_output;
            {
                coutRedirect redirect;
                // Определяем имя файла БД при первой команде open
                if (wmessage.substr(0, 4) == "open") {
                    std::string filename = wmessage.substr(5); // open <filename>
                    std::lock_guard<std::mutex> lock(db_map_mutex);
                    current_db_file = filename;
                    // Создаём новый экземпляр Database для клиента
                    db_ptr = std::make_shared<Database>();
                    db_ptr->selectDB(filename);
                    // Регистрируем колбэк для уведомлений
                    db_ptr->notifyOnChange([filename, clientSocket]() {
                        // Уведомляем всех клиентов с этим файлом, кроме инициатора
                        std::lock_guard<std::mutex> lock2(clients_mutex);
                        auto it = file_clients_map.find(filename);
                        if (it != file_clients_map.end()) {
                            for (int sock : it->second) {
                                if (sock == clientSocket) continue;
                                int update_code = -1;
                                send(sock, &update_code, sizeof(int), 0);
                            }
                        }
                    });
                    db_instances_map[filename].push_back(db_ptr);
                    // Зарегистрировать клиента для этого файла
                    {
                        std::lock_guard<std::mutex> lock2(clients_mutex);
                        file_clients_map[filename].insert(clientSocket);
                    }
                    captured_output = redirect.getOutput();
                }
                else if (!db_ptr) {
                    // Если не был выполнен open, игнорируем команду
                    captured_output = "Сначала выполните команду open <файл>";
                } else {
                    db_ptr->parseCommand(wmessage);
                    captured_output = redirect.getOutput();
                }
            }
            std::string message = captured_output;
            int respLength = message.size();
            send(clientSocket, &respLength, sizeof(int), 0);
            send(clientSocket, message.c_str(), respLength, 0);
        } catch (const std::bad_alloc&) {
            std::cerr << "\033[1;31mОшибка выделения памяти (bad_alloc)\033[0m\n";
            close(clientSocket);
            return;
        }
    }
    // Удаляем клиента из file_clients_map и db_instances_map
    if (!current_db_file.empty()) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it = file_clients_map.find(current_db_file);
        if (it != file_clients_map.end()) {
            it->second.erase(clientSocket);
            if (it->second.empty()) file_clients_map.erase(it);
        }
        // Удаляем экземпляр Database для этого клиента
        auto& arr = db_instances_map[current_db_file];
        arr.erase(std::remove_if(arr.begin(), arr.end(), [&](const std::shared_ptr<Database>& db) { return db == db_ptr; }), arr.end());
        db_ptr->clearCallbacks();
        if (arr.empty()) db_instances_map.erase(current_db_file);
    }
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        client_sockets.erase(clientSocket);
    }
    close(clientSocket);
}

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));
    std::cout.imbue(std::locale(std::cout.getloc(), new NoSeparator));
    std::map<std::string, std::string> config = read_config("server_config.ini");
    int port = std::stoi(config["port"]);
    int max_clients = std::stoi(config["max_clients"]);

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "\033[1;31mОшибка создания сокета\033[0m\n";
        return 1;
    }
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "\033[1;31mОшибка установки опции SO_REUSEADDR\033[0m\n";
        close(serverSocket);
        return 1;
    }

    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "\033[1;31mОшибка привязки сокета\033[0m\n";
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, max_clients) < 0) {
        std::cerr << "\033[1;31mОшибка прослушивания\033[0m\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "Сервер запущен на порту " << port << ". Ожидание подключений...\n";

    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            std::cerr << "\033[1;31mОшибка принятия подключения\033[0m\n";
            continue;
        }
        std::cout << "Новый клиент подключен: " << inet_ntoa(clientAddr.sin_addr) << std::endl;
        std::thread([clientSocket]() { handle_client(clientSocket); }).detach();
        std::cout << "Ожидание новых подключений...\n";
    }
    close(serverSocket);
    return 0;
}
