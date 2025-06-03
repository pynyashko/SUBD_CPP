#pragma once
#include <cwchar>
#include <iconv.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <map>
#include <regex>
#include <limits>
#include <set>
#include <unordered_set>
#include <cstring>
#include <codecvt>
#include <functional>

// -------------------------------------------------- Функции перевода строк из разных кодировок --------------------------------------------------
// Конвертация UTF-8 (файл) → UTF-16 (в памяти)
std::wstring utf8_to_utf16(const std::string& utf8);
// Конвертация UTF-16 → UTF-8 (для имени файла)
std::string utf16_to_utf8(const std::wstring& utf16);

// Создаём свою facet-локаль, которая убирает разделители тысяч
class NoSeparator : public std::numpunct<char> {
protected:
    char do_thousands_sep() const override { return '\0'; }
    std::string do_grouping() const override { return ""; }
};
// Кастомный facet для wide-потоков
class NoWSeparator : public std::numpunct<wchar_t> {
protected:
    wchar_t do_thousands_sep() const override { return L'\0'; }
    std::string do_grouping() const override { return ""; }
};

// Валидация ФИО (три слова, кириллица, с заглавной буквы)
bool validate_name(const std::wstring& name);
// Валидация группы (целое число > 0)
bool validate_group(int group);
// Валидация оценки (2.0 <= x <= 5.0)
bool validate_rating(double rating);

// Ядро БД
class Database {
private:
    // Представление сущности студента в БД
    struct Student {
        int id; // уникальный идентификатор
        wchar_t name[64];
        int group;
        double rating;
        std::wstring info;
    };
private:
    std::vector<Student> students;                   // Все записи
    enum class Index : size_t {};
    struct CompareByName {                                                       // Компаратор для дерева ФИО
        using is_transparent = void;                                             //
        const std::vector<Student>* students_ptr;                                //
        bool operator()(Index a, const wchar_t* b) const;                        //
        bool operator()(const wchar_t* a, Index b) const;                        //
        bool operator()(Index a, Index b) const;                                 //
    };                                                                           //
    std::set<Index, CompareByName> studentsBN{ CompareByName{&students} };       // Записи в дереве по ФИО

    struct CompareByGroup {                                                      // Компаратор для дерева Группы
        using is_transparent = void;                                             //
        const std::vector<Student>* students_ptr;                                //
        bool operator()(Index a, int group) const;                               //
        bool operator()(int group, Index b) const;                               //
        bool operator()(Index a, Index b) const;                                 //
    };                                                                           //
    std::set<Index, CompareByGroup> studentsBG{ CompareByGroup{&students} };     // Записи в дереве по Группе

    struct CompareByRating {                                                     // Компаратор для дерева Оценки
        using is_transparent = void;                                             //
        const std::vector<Student>* students_ptr;                                //
        bool operator()(Index a, double rating) const;                           //
        bool operator()(double rating, Index b) const;                           //
        bool operator()(Index a, Index b) const;                                 //
    };                                                                           //
    std::set<Index, CompareByRating> studentsBR{ CompareByRating{&students} };   // Записи в дереве по Оценке

    std::vector<size_t> selectedStudents;            // Выбранные записи 
    std::wstring dbFile;  // Имя файла базы данных
    int nextId = 1; // для генерации новых id
    size_t version = 0; // версия БД, увеличивается при каждом изменении
    std::vector<std::function<void()>> changeCallbacks; // колбэки для оповещения

    // -------------------------------------------------- Приватные функции-помощники --------------------------------------------------
    // Загрузка БД из файла
    void loadFromFile(const std::wstring& filename);

    // Сохранение БД в файл
    void saveToFile(const std::wstring& filename);

    // Парсинг критериев из команды
    // (строка "name=Кузьмин* group=101-103" разобьется на пары ключ-значение: [field]: value (["name"]: "Кузьмин*", ["group"]: "101-103"))
    std::map<std::wstring, std::wstring> parseCriteria(const std::wstring& command) const;

    // Проверка соответствия записи критериям (тем самым парам ключ-значение)
    bool matchesCriteria(const Student& student, const std::map<std::wstring, std::wstring>& criteria) const;

public:
    size_t getVersion() const;
    void clearCallbacks();
    void notifyOnChange(std::function<void()> callback);
    void notifyChanged();

public:
    // Сортировка записей по очереди: group, name, rating, info
    void sort();

    // Выполнение команды из строки
    void parseCommand(const std::wstring& full_command);

    // -------------------------------------------------- Работа с файлом БД --------------------------------------------------
    // Выбор файла базы данных
    void selectDB(const std::wstring& filename);                      // open       <название файла>

    // Сохранение базы данных
    void saveDB();                                                    // save

    // -------------------------------------------------- Выборка из данных --------------------------------------------------
    // Выборка записей
    void select(const std::wstring& command);                         // select     <id=<...>, name=<...>, group=<...>, rating=<...>>

    // Повторная выборка среди выбранных записей
    void reselect(const std::wstring& command);                       // reselect   <id=<...>, <name=<...>, group=<...>, rating=<...>>

    // Вывод выбранных записей
    void print(const std::wstring& fields) const;                     // print      <name, group, rating, info> [sort <name/group/rating>]

    // Редактирование выбранных записей (всех)
    void update(const std::wstring& command);                         // update     <name=<...>, group=<...>, rating=<...>>

    // Удаление выбранных записей
    void remove();                                                    // remove

    // Добавление записи
    void add(const std::wstring& command);                            // add        <фио>\t<группа>\t<оценка>\t<инфа>
};

/// Допустимые команды и их использование:
/// 
///    +-----------+--------------------------------------------------------------------------+---------------------------------------------------------------+
///    | Команда   | Сигнатура                                                                | Описание                                                      |
///    +-----------+--------------------------------------------------------------------------+---------------------------------------------------------------+
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
