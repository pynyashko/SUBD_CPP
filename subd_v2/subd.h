#pragma once
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


// Создаём свою facet-локаль, которая убирает разделители тысяч
class NoSeparator : public std::numpunct<char> {
protected:
    char do_thousands_sep() const override { return '\0'; }
    std::string do_grouping() const override { return ""; }
};

// Валидация ФИО (три слова, кириллица, с заглавной буквы)
bool validate_name(const std::string& name);
// Валидация группы (целое число > 0)
bool validate_group(int group);
// Валидация оценки (2.0 <= x <= 5.0)
bool validate_rating(double rating);

// Яро БД
class Database {
private:
    // Представление сущности студента в БД
    struct Student {
        int id; // уникальный идентификатор
        char name[64];
        int group;
        double rating;
        std::string info;
    };
private:
    std::vector<Student> students;                   // Все записи
    enum class Index : size_t {};
    struct CompareByName {                                                       // Компаратор для дерева ФИО
        using is_transparent = void;                                             //
        const std::vector<Student>* students_ptr;                                //
        bool operator()(Index a, const char* b) const;                        //
        bool operator()(const char* a, Index b) const;                        //
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
    std::string dbFile;  // Имя файла базы данных
    int nextId = 1; // для генерации новых id
    size_t version = 0; // версия БД, увеличивается при каждом изменении
    std::vector<std::function<void()>> changeCallbacks; // колбэки для оповещения

    // -------------------------------------------------- Приватные функции-помощники --------------------------------------------------
    // Загрузка БД из файла
    void loadFromFile(const std::string& filename);

    // Сохранение БД в файл
    void saveToFile(const std::string& filename);

    // Парсинг критериев из команды
    // (строка "name=Кузьмин* group=101-103" разобьется на пары ключ-значение: [field]: value (["name"]: "Кузьмин*", ["group"]: "101-103"))
    std::map<std::string, std::string> parseCriteria(const std::string& command) const;

    // Проверка соответствия записи критериям (тем самым парам ключ-значение)
    bool matchesCriteria(const Student& student, const std::map<std::string, std::string>& criteria) const;

public:
    size_t getVersion() const;
    void clearCallbacks();
    void notifyOnChange(std::function<void()> callback);
    void notifyChanged();

public:
    // Сортировка записей по очереди: group, name, rating, info
    void sort();

    // Выполнение команды из строки
    void parseCommand(const std::string& full_command);

    // -------------------------------------------------- Работа с файлом БД --------------------------------------------------
    // Выбор файла базы данных
    void selectDB(const std::string& filename);                      // open       <название файла>

    // Сохранение базы данных
    void saveDB();                                                    // save

    // -------------------------------------------------- Выборка из данных --------------------------------------------------
    // Выборка записей
    void select(const std::string& command);                         // select     <id=<...>, name=<...>, group=<...>, rating=<...>>

    // Повторная выборка среди выбранных записей
    void reselect(const std::string& command);                       // reselect   <id=<...>, <name=<...>, group=<...>, rating=<...>>

    // Вывод выбранных записей
    void print(const std::string& fields) const;                     // print      <name, group, rating, info> [sort <name/group/rating>]

    // Редактирование выбранных записей (всех)
    void update(const std::string& command);                         // update     <name=<...>, group=<...>, rating=<...>>

    // Удаление выбранных записей
    void remove();                                                    // remove

    // Добавление записи
    void add(const std::string& command);                            // add        <фио>\t<группа>\t<оценка>\t<инфа>
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
