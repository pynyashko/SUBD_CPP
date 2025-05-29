#include "subd.h"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

const wchar_t* HELP_INFO = LR"(/// Допустимые команды и их использование:
/// 
///    +-----------+--------------------------------------------------------------------------+---------------------------------------------------------------+
///    | Команда   | Сигнатура                                                                | Описание                                                      |
///    +-----------+--------------------------------------------------------------------------+---------------------------------------------------------------+
///    | help      |                                                                          | Вывести памятку о пользовании                                 |
///    | clear     |                                                                          | Очистка консоли                                               |
///    | reconnect |                                                                          | Переподключение к серверу (все несохраненные данные пропадут) |
///    | exit      |                                                                          | Закрыть БД (все несохраненные данные пропадут)                |
///    | open      | <название файла>                                                         | Выбор файла базы данных                                       |
///    | save      |                                                                          | Сохранение базы данных                                        |
///    | select    | [id=<...> name=<...>, group=<...>, rating=<...>]                         | Выборка записей                                               |
///    | reselect  | [id=<...> name=<...>, group=<...>, rating=<...>]                         | Повторная выборка среди выбранных записей                     |
///    | update    | <name=<...>, group=<...>, rating=<...>, info=<...>>                      | Редактирование выбранных записей (всех)                       |
///    | remove    |                                                                          | Удаление выбранных записей                                    |
///    | add       | <фио>\t<группа>\t<оценка>\t<инфа>                                        | Добавление записи                                             |
///    | print     | [id, name, group, rating, info] [range=<...>] [sort <name/group/rating>] | Вывод выбранных записей                                       |
///    +-----------+--------------------------------------------------------------------------+---------------------------------------------------------------+

/// Пример допустимых значений для поиска по полям (select, reselect, update, remove):
/// 
///    +--------+--------------------------------------------------------------------+
///    |  поле  |                       Допустимые значения                          |
///    +--------+---+-------+--------------+-----------+-----------------------------+
///    |   id   | * |   1   |     1-100    |   *-100   |             1-*             |
///    +--------+---+-------+--------------+-----------+-----------------------------+
///    |  name  | * |  "*"  |  "Кузьмин *" |  Ку*-Пе*  |   "Кузьмин Иван Иванович"   |
///    +--------+---+-------+--------------+-----------+-----------------------------+
///    | group  | * |  101  |    101-103   |   *-105   |            104-*            |
///    +--------+---+-------+--------------+-----------+-----------------------------+
///    | rating | * |   4   |      4-5     |    *-4    |             4-*             |
///    +--------+---+-------+--------------+-----------+-----------------------------+

/// Описание полей для студента:
/// 
///    +--------+----------------------------------------------------------------------------------+
///    |  поле  |                               Допустимые значения                                |
///    +--------+----------------------------------------------------------------------------------+
///    |  name  | Фамилия Имя Отчество (Все с заглавной, разделитель - пробел, буквы - кириллица)  |
///    +--------+----------------------------------------------------------------------------------+
///    | group  |      Группа (Целое положительное число, диапазон выбирается самостоятельно)      |
///    +--------+----------------------------------------------------------------------------------+
///    | rating |    Оценка (Дробное число в диапазоне от 2 до 5 с одной цифрой после запятой)     |
///    +--------+----------------------------------------------------------------------------------+
///    |  info  | Дополнительная информация (Обычный текст на кириллице/латинице с цифрами и т.д.) |
///    +--------+----------------------------------------------------------------------------------+
)";

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

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));
    std::wcout.imbue(std::locale(std::wcout.getloc(), new NoWSeparator));
    system("clear");
    std::map<std::string, std::string> config = read_config("client_config.ini");
    std::string server_ip = config["server_ip"];
    int port = std::stoi(config["port"]);

    while (true) {
        int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket < 0) {
            std::wcerr << L"\033[1;31mОшибка создания сокета\033[0m\n";
            return 1;
        }

        struct sockaddr_in serverAddr;
        std::memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        if (inet_pton(AF_INET, server_ip.c_str(), &serverAddr.sin_addr) <= 0) {
            std::wcerr << L"\033[1;31mНеверный адрес\033[0m\n";
            close(clientSocket);
            return 1;
        }

        std::wcout << L"Подключение к серверу " << utf8_to_utf16(server_ip) << L":" << port << L"...\n";
        if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::wcerr << L"\033[1;31mОшибка подключения. Повторная попытка через 5 секунд...\033[0m\n";
            close(clientSocket);
            sleep(5);
            continue;
        }

        std::wcout << L"Подключено к серверу!\n";

        while (true) {
            std::wstring wmessage;
            std::wcout << L"\033[1;36mcommand-> \033[0;35m";
            std::getline(std::wcin, wmessage);
            std::wcout << L"\033[0m";

            if (wmessage == L"help") {
                std::wcout << L"\033[33m" << HELP_INFO << L"\033[0m\n";
                continue;
            }
            else if (wmessage == L"clear") {
                system("clear");
                continue;
            }
            else if (wmessage == L"reconnect") {
                close(clientSocket);
                break;
            }
            else if (wmessage == L"exit") {
                close(clientSocket);
                return 0;
            }
            // Проверяем уведомление перед отправкой команды
            int respLengthPeek = 0;
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(clientSocket, &readfds);
            struct timeval tv = {0, 0}; // не блокировать
            int ready = select(clientSocket + 1, &readfds, NULL, NULL, &tv);
            if (ready > 0 && FD_ISSET(clientSocket, &readfds)) {
                ssize_t bytesReadPeek = recv(clientSocket, &respLengthPeek, sizeof(int), MSG_PEEK);
                if (bytesReadPeek > 0 && respLengthPeek == -1) {
                    // Считать уведомление
                    recv(clientSocket, &respLengthPeek, sizeof(int), 0);
                    std::wcerr << L"\033[1;33mБаза данных была изменена другим пользователем. Вы точно хотите выполнить эту команду? Результат может быть непредсказуемым. (y/n): \033[0m";
                    std::wstring confirm;
                    std::getline(std::wcin, confirm);
                    if (confirm != L"y" && confirm != L"Y" && confirm != L"д" && confirm != L"Д") {
                        std::wcout << L"\033[1;33mКоманда не отправлена. Введите команду заново.\033[0m\n";
                        continue;
                    }
                }
            }

            // Конвертируем в UTF-8 для передачи
            std::string message = utf16_to_utf8(wmessage);
            int msgLength = message.size();  // Размер в байтах (не в символах!)
            
            if (send(clientSocket, &msgLength, sizeof(int), 0) < 0) {
                std::wcerr << L"\033[1;31mОшибка отправки длины сообщения\033[0m\n";
                break;
            }

            if (send(clientSocket, message.c_str(), msgLength, 0) < 0) {
                std::wcerr << L"\033[1;31mОшибка отправки сообщения\033[0m\n";
                break;
            }

            // Получаем ответ
            int respLength;
            ssize_t bytesRead = recv(clientSocket, &respLength, sizeof(int), 0);
            if (bytesRead <= 0) {
                std::wcerr << L"\033[1;31mСервер отключился\033[0m\n";
                break;
            }
            if (respLength == -1) {
                std::wcerr << L"\033[1;33m...База данных была изменена другим пользователем. Повторите выборку или обновите данные.\033[0m\n";
                continue;
            }
            char* buffer = new char[respLength + 1];
            int totalReceived = 0;
            while (totalReceived != respLength) {
                bytesRead = recv(clientSocket, buffer + totalReceived, respLength - totalReceived, 0);
                if (bytesRead <= 0) {
                    std::wcerr << L"\033[1;31mОшибка получения ответа\033[0m\n";
                    delete[] buffer;
                    close(clientSocket)
                    break;
                }
                totalReceived += bytesRead;
            }
            buffer[respLength] = '\0';

            // Конвертируем обратно в UTF-16
            std::wstring wresponse = utf8_to_utf16(buffer);
            std::wcout << L"\033[33m" << wresponse << L"\033[0m";
            delete[] buffer;
        }
    }

    return 0;
}
