#include "subd.h"
#include <regex>
#include <typeinfo>


// -------------------------------------------------- Компараторы для деревьев --------------------------------------------------
// ------------------- Реализация CompareByName -------------------
bool Database::CompareByName::operator()(Index a, const char* b) const {
    size_t len = strlen(b) - 1;
    if (b[len] == '*')
        return strncmp((*students_ptr)[(size_t)a].name, b, len) < 0;
    else
        return strcmp((*students_ptr)[(size_t)a].name, b) < 0;
}
bool Database::CompareByName::operator()(const char* a, Index b) const {
    size_t len = strlen(a) - 1;
    if (a[len] == '*')
        return strncmp(a, (*students_ptr)[(size_t)b].name, len) < 0;
    else
        return strcmp(a, (*students_ptr)[(size_t)b].name) < 0;
}
bool Database::CompareByName::operator()(Index a, Index b) const {
    int res = strcmp((*students_ptr)[(size_t)a].name,
                     (*students_ptr)[(size_t)b].name);
    if (res)
        return res < 0;
    return a < b;
}
// ------------------- Реализация CompareByGroup -------------------
bool Database::CompareByGroup::operator()(Index a, int group) const {
    return (*students_ptr)[(size_t)a].group < group;
}
bool Database::CompareByGroup::operator()(int group, Index b) const {
    return group < (*students_ptr)[(size_t)b].group;
}
bool Database::CompareByGroup::operator()(Index a, Index b) const {
    if ((*students_ptr)[(size_t)a].group !=
        (*students_ptr)[(size_t)b].group)
        return (*students_ptr)[(size_t)a].group <
               (*students_ptr)[(size_t)b].group;
    return a < b;
}
// ------------------- Реализация CompareByRating -------------------
bool Database::CompareByRating::operator()(Index a, double rating) const {
    return (*students_ptr)[(size_t)a].rating < rating;
}
bool Database::CompareByRating::operator()(double rating, Index b) const {
    return rating < (*students_ptr)[(size_t)b].rating;
}
bool Database::CompareByRating::operator()(Index a, Index b) const {
    if ((*students_ptr)[(size_t)a].rating !=
        (*students_ptr)[(size_t)b].rating)
        return (*students_ptr)[(size_t)a].rating <
               (*students_ptr)[(size_t)b].rating;
    return a < b;
}

// Валидация ФИО (три слова, кириллица, с заглавной буквы)
bool validate_name(const std::string& name) {
    std::regex re("(^[А-ЯЁ][а-яё]+ [А-ЯЁ][а-яё]+ [А-ЯЁ][а-яё]+$)");
    return std::regex_match(name, re);
}
// Валидация группы (целое число > 0)
bool validate_group(int group) {
    return group > 0;
}
// Валидация оценки (2.0 <= x <= 5.0)
bool validate_rating(double rating) {
    return rating >= 2.0 && rating <= 5.0;
}


// -------------------------------------------------- Приватные функции-помощники --------------------------------------------------
// Загрузка бд из файла
void Database::loadFromFile(const std::string& filename) {
    // Открываем файл для чтения в UTF-8
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Ошибка: не удалось открыть файл " << filename << "\n";
        return;
    }

    // Очищаем существующие данные
    students.clear();
    studentsBN.clear();
    studentsBG.clear();
    studentsBR.clear();
    selectedStudents.clear();
    nextId = 1;

    Student temp;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string id_str, name;
        std::getline(iss, id_str, '\t');
        bool has_id = std::all_of(id_str.begin(), id_str.end(), ::isdigit);
        if (has_id) {
            temp.id = std::stoi(id_str);
            std::getline(iss, name, '\t');
        }
        else {
            temp.id = nextId++;
            name = id_str;
        }
        // Копируем имя в temp.name, учитывая максимальную длину 64
        strncpy(temp.name, name.c_str(), 63);
        temp.name[63] = '\0'; // Гарантируем завершающий нуль

        iss >> temp.group >> temp.rating;
        iss.ignore(1);
        std::getline(iss, temp.info);

        students.push_back(temp);
        studentsBN.insert(Index{ students.size() - 1 });
        studentsBG.insert(Index{ students.size() - 1 });
        studentsBR.insert(Index{ students.size() - 1 });
        if (temp.id >= nextId) nextId = temp.id + 1;
    }

    selectedStudents.reserve(students.size());
    selectedStudents.resize(students.size());
    for (size_t i = 0; i < students.size(); ++i)
        selectedStudents[i] = i;

    file.close();
    std::cout << "База данных загружена из " << filename << "(" << students.size() << ")\n";
}
// Сохранение БД в файл
void Database::saveToFile(const std::string& filename) {
    // Открываем файл для записи в UTF-8
    std::ofstream file(filename);
    file.imbue(std::locale(file.getloc(), new NoSeparator));
    if (!file.is_open()) {
        std::cout << "Ошибка: не удалось сохранить файл " << filename << "\n";
        return;
    }

    for (const auto& student : students) {
        std::string name_str(student.name);
        std::string name_utf8 = name_str;
        std::string info_utf8 = student.info;
        file << student.id << "\t" << name_utf8 << "\t" << student.group << "\t" << student.rating << "\t" << info_utf8 << "\n";
    }

    file.close();
    notifyChanged();
}
// Парсинг критериев из команды (строка "name=Кузьмин* group=101-103" разобьется на пары ключ-значение: [field]: value (["name"]: "Кузьмин*", ["group"]: "101-103"))
std::map<std::string, std::string> Database::parseCriteria(const std::string& command) const {
    std::map<std::string, std::string> result;

    // Регулярное выражение для поиска пар ключ=значение
    std::regex pattern(R"((\w+)=("[^"]*"|[^\s]*))");
    std::sregex_iterator it(command.begin(), command.end(), pattern);
    std::sregex_iterator end;

    while (it != end) {
        std::smatch match = *it;
        std::string key = match[1].str();
        std::string value = match[2].str();

        // Удаляем кавычки, если они есть
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        result[key] = value;
        ++it;
    }

    return result;
}
// Проверка соответствия записи критериям(тем самым парам ключ-значение)
bool Database::matchesCriteria(const Student& student, const std::map<std::string, std::string>& criteria) const {
    for (const auto& crit : criteria) {
        const std::string& field = crit.first;
        const std::string& value = crit.second;

        if (value == "*")
            continue;

        if (field == "id") {
            int id = student.id;
            size_t dashPos = value.find('-');
            if (dashPos != std::string::npos) {
                std::string startStr = value.substr(0, dashPos);
                std::string endStr = value.substr(dashPos + 1);
                int start = startStr == "*" ? std::numeric_limits<int>::min() : std::stoi(startStr);
                int end = endStr == "*" ? std::numeric_limits<int>::max() : std::stoi(endStr);
                if (id < start || id > end) {
                    return false;
                }
            }
            else {
                if (std::to_string(id) != value) {
                    return false;
                }
            }
        }
        else if (field == "name") {
            // Заменяем * в критерии на .* (в regex это "любое количество любых символов")
            std::string regexPattern = "^"; // ^ - начало строки
            size_t lastPos = 0;
            size_t starPos = value.find('*');

            // Разбиваем критерий на части и заменяем * на .*
            while (starPos != std::string::npos) {
                regexPattern += value.substr(lastPos, starPos - lastPos) + ".*";
                lastPos = starPos + 1;
                starPos = value.find('*', lastPos);
            }
            regexPattern += value.substr(lastPos) + "$"; // $ - конец строки

            // Создаём regex и проверяем совпадение
            std::regex re(regexPattern);
            if (!std::regex_search(student.name, re)) {
                return false;
            }
        }
        else if (field == "group") {
            int group = student.group;
            size_t dashPos = value.find('-');
            if (dashPos != std::string::npos) {
                std::string startStr = value.substr(0, dashPos);
                std::string endStr = value.substr(dashPos + 1);
                int start = startStr == "*" ? std::numeric_limits<int>::min() : std::stoi(startStr);
                int end = endStr == "*" ? std::numeric_limits<int>::max() : std::stoi(endStr);
                if (group < start || group > end) {
                    return false;
                }
            }
            else {
                if (std::to_string(group) != value) {
                    return false;
                }
            }
        }
        else if (field == "rating") {
            double rating = student.rating;
            size_t dashPos = value.find('-');
            if (dashPos != std::string::npos) {
                std::string startStr = value.substr(0, dashPos);
                std::string endStr = value.substr(dashPos + 1);
                double start = startStr == "*" ? -std::numeric_limits<double>::max() : std::stod(startStr);
                double end = endStr == "*" ? std::numeric_limits<double>::max() : std::stod(endStr);
                if (rating < start || rating > end) {
                    return false;
                }
            }
            else {
                if (std::to_string(rating) != value) {
                    return false;
                }
            }
        }
    }
    return true;
}

// -------------------------------------------------- Внешние методы работы с БД --------------------------------------------------
// Выполнение команды из строки
void Database::parseCommand(const std::string& full_command) {
    std::string command;
    std::string args;
    if (size_t space = full_command.find(" "); space == std::string::npos) {
        command = full_command;
        args = "";
        if (command == "open" ||
            command == "add" ||
            command == "update") {
            std::cout << "Не удалось обработать команду\n";
            return;
        }
    }
    else {
        command = full_command.substr(0, space);
        args = full_command.substr(space + 1, full_command.length() - space - 1);
    }
    if (command == "open") {
        selectDB(args);
    }
    else if (command == "save") {
        saveDB();
    }
    else if (command == "select") {
        select(args);
    }
    else if (command == "reselect") {
        reselect(args);
    }
    else if (command == "print") {
        print(args);
    }
    else if (command == "add") {
        add(args);
    }
    else if (command == "remove") {
        remove();
    }
    else if (command == "update") {
        update(args);
    }
    else {
        std::cout << "Не удалось обработать команду\n";
    }
}
// -------------------------------------------------- Работа с файлом БД --------------------------------------------------
// Выбор файла базы данных
void Database::selectDB(const std::string& filename) {
    dbFile = filename;
    loadFromFile(dbFile);
    notifyChanged();
}
// Сохранение базы данных
void Database::saveDB() {
    saveToFile(dbFile);
    std::cout << "База данных сохранена в " << dbFile << "\n";
}
// -------------------------------------------------- Выборка из данных --------------------------------------------------
// Выборка записей
void Database::select(const std::string& command) {
    auto criteria = parseCriteria(command);
    if (criteria.empty()) {
        selectedStudents.clear();
        selectedStudents.reserve(students.size());
        selectedStudents.resize(students.size());
        for (size_t i = 0; i < students.size(); ++i) {
            selectedStudents[i] = i;
        }
        std::cout << "Выбрано " << selectedStudents.size() << " записей после выборки\n";
        return;
    }
    selectedStudents.clear();

    // --- Быстрый поиск по деревьям ---
    std::set<Index>::iterator startN, endN, startG, endG, startR, endR; // Диапазоны валидных записей по деревьям
    bool N{}, G{}, R{}; // Были ли найдены записи по этим деревьям
    std::string id_criteria;
    for (const auto& crit : criteria) {
        const std::string& field = crit.first;
        const std::string& value = crit.second;
        if (field == "id") {
            id_criteria = value;
        }
        // --- Поиск по name ---
        else if (field == "name") {
            if (value == "*") continue;
            size_t dashPos = value.find('-');
            if (dashPos != std::string::npos) { // Если у нас поиск по диапазону значений (name=А*-Б*, ...)
                std::string startStr = value.substr(0, dashPos);
                std::string endStr = value.substr(dashPos + 1);
                if (startStr > endStr)
                    if (startStr == "*" && endStr == "*") continue;
                if (size_t starPos = startStr.find("*"); starPos != std::string::npos && starPos != startStr.length() - 1) continue;
                if (startStr == "*") startN = studentsBN.begin();
                else startN = studentsBN.equal_range(startStr.c_str()).first;
                if (size_t starPos = endStr.find("*"); starPos != std::string::npos && starPos != endStr.length() - 1) continue;
                if (endStr == "*") endN = studentsBN.end();
                else endN = studentsBN.equal_range(endStr.c_str()).second;
            }
            else { // Если у нас поиск по одному значению
                if (size_t starPos = value.find("*"); starPos != std::string::npos && starPos != value.length() - 1) continue;
                auto [f, s] = studentsBN.equal_range(value.c_str());
                startN = f;
                endN = s;
            }
            N = true;
        }
        // --- Поиск по group ---
        else if (field == "group") {
            if (value == "*") continue;
            size_t dashPos = value.find('-');
            if (dashPos != std::string::npos) { // Если у нас поиск по диапазону значений (group=101-105, ...)
                std::string startStr = value.substr(0, dashPos);
                std::string endStr = value.substr(dashPos + 1);
                if (startStr == "*" && endStr == "*") continue;
                if (startStr =="*") startG = studentsBG.begin();
                else startG = studentsBG.equal_range(std::stoi(startStr)).first;
                if (endStr == "*") endG = studentsBG.end();
                else endG = studentsBG.equal_range(std::stoi(endStr)).second;
            }
            else { // Если у нас поиск по одному значению
                auto [f, s] = studentsBG.equal_range(std::stoi(value));
                startG = f;
                endG = s;
            }
            G = true;
        }
        // --- Поиск по rating ---
        else if (field == "rating") {
            if (value == "*") continue;
            size_t dashPos = value.find(L'-');
            if (dashPos != std::string::npos) { // Если у нас поиск по диапазону значений (rating=3-5, ...)
                std::string startStr = value.substr(0, dashPos);
                std::string endStr = value.substr(dashPos + 1);
                if (startStr == "*" && endStr == "*") continue;
                if (startStr == "*") startR = studentsBR.begin();
                else startR = studentsBR.equal_range(std::stod(startStr)).first;
                if (endStr == "*") endR = studentsBR.end();
                else endR = studentsBR.equal_range(std::stod(endStr)).second;
            }
            else { // Если у нас поиск по одному значению
                auto [f, s] = studentsBR.equal_range(std::stod(value));
                startR = f;
                endR = s;
            }
            R = true;
        }
    }
    // --- Если нет критериев — выбрать всё ---
    if (!N && !G && !R && id_criteria.empty()) {
        selectedStudents.reserve(students.size());
        selectedStudents.resize(students.size());
        for (size_t i = 0; i < students.size(); ++i)
            selectedStudents[i] = i;
        std::cout << "Выбрано " << selectedStudents.size() << " записей после выборки\n";
        return;
    }
    // --- Собираем индексы из каждого диапазона ---
    std::set<size_t> name_indices, group_indices, rating_indices;
    if (N) {
        for (auto it = startN; it != endN; ++it) {
            if (it == studentsBN.end()) {
                name_indices.clear();
                break;
            }
            name_indices.insert((size_t)*it);
        }
    }
    if (G) {
        for (auto it = startG; it != endG; ++it) {
            if (it == studentsBG.end()) {
                group_indices.clear();
                break;
            }
            group_indices.insert((size_t)*it);
        }
    }
    if (R) {
        for (auto it = startR; it != endR; ++it) {
            if (it == studentsBR.end()) {
                rating_indices.clear();
                break;
            }
            rating_indices.insert((size_t)*it);
        }
    }
    std::vector<size_t> temp_result;
    // --- Находим пересечение всех непустых множеств ---
    if (!name_indices.empty() && !group_indices.empty() && !rating_indices.empty()) {
        std::set_intersection(name_indices.begin(), name_indices.end(), group_indices.begin(), group_indices.end(), std::back_inserter(temp_result));
        std::set<size_t> temp_set(temp_result.begin(), temp_result.end());
        temp_result.clear();
        std::set_intersection(temp_set.begin(), temp_set.end(), rating_indices.begin(), rating_indices.end(), std::back_inserter(temp_result));
    }
    else if (!name_indices.empty() && !group_indices.empty()) {
        std::set_intersection(name_indices.begin(), name_indices.end(), group_indices.begin(), group_indices.end(), std::back_inserter(temp_result));
    }
    else if (!name_indices.empty() && !rating_indices.empty()) {
        std::set_intersection(name_indices.begin(), name_indices.end(), rating_indices.begin(), rating_indices.end(), std::back_inserter(temp_result));
    }
    else if (!group_indices.empty() && !rating_indices.empty()) {
        std::set_intersection(group_indices.begin(), group_indices.end(), rating_indices.begin(), rating_indices.end(), std::back_inserter(temp_result));
    }
    else if (!name_indices.empty()) {
        temp_result.assign(name_indices.begin(), name_indices.end());
    }
    else if (!group_indices.empty()) {
        temp_result.assign(group_indices.begin(), group_indices.end());
    }
    else if (!rating_indices.empty()) {
        temp_result.assign(rating_indices.begin(), rating_indices.end());
    }
    // --- Если был хотя бы один критерий по дереву, temp_result содержит кандидатов ---
    // --- Если есть критерий по id — фильтруем temp_result по id, иначе просто копируем ---
    if (!id_criteria.empty()) {
        std::vector<size_t> id_filtered;
        for (size_t idx : temp_result) {
            std::map<std::string, std::string> id_map = { {"id", id_criteria} };
            if (matchesCriteria(students[idx], id_map)) id_filtered.push_back(idx);
        }
        selectedStudents = id_filtered;
    }
    else {
        selectedStudents.insert(selectedStudents.end(), temp_result.begin(), temp_result.end());
    }
    // --- Если не было критериев по деревьям, но был только id — ищем по id по всем студентам ---
    if ((N || G || R) == false && !id_criteria.empty()) {
        for (size_t i = 0; i < students.size(); ++i) {
            std::map<std::string, std::string> id_map = { {"id", id_criteria} };
            if (matchesCriteria(students[i], id_map)) selectedStudents.push_back(i);
        }
    }
    std::cout << "Выбрано " << selectedStudents.size() << " записей после выборки\n";
}
// Повторная выборка
void Database::reselect(const std::string& command) {
    if (selectedStudents.empty()) {
        std::cout << "Нет выбранных записей для повторной выборки\n";
        return;
    }
    auto criteria = parseCriteria(command);
    if (criteria.empty()) {
        std::cout << "Выбрано " << selectedStudents.size() << " записей после повторной выборки\n";
        return;
    }
    std::vector<size_t> temp;
    for (size_t i = 0; i < selectedStudents.size(); ++i) {
        if (matchesCriteria(students[selectedStudents[i]], criteria)) {
            temp.push_back(selectedStudents[i]);
        }
    }
    selectedStudents = temp;
    std::cout << "Выбрано " << selectedStudents.size() << " записей после повторной выборки\n";
}
// Вывод выбранных записей
void Database::print(const std::string& fields) const {
    std::vector<size_t> output_students = selectedStudents;
    std::string sort_value;
    size_t sort_pos = fields.find("sort");
    if (sort_pos != std::string::npos) {
        sort_value = fields.substr(sort_pos + 4);
        sort_value.erase(0, sort_value.find_first_not_of(" "));
        sort_value.erase(sort_value.find_last_not_of(" ") + 1);
    }
    if (!sort_value.empty()) {
        std::sort(output_students.begin(), output_students.end(), [&](size_t a, size_t b) {
            if (sort_value == "name")
                return strcmp(students[a].name, students[b].name) <= 0;
            else if (sort_value == "group")
                return students[a].group < students[b].group;
            else if (sort_value == "rating")
                return students[a].rating < students[b].rating;
            else
                return strcmp(students[a].name, students[b].name) <= 0;
            });
    }
    // --- Поддержка диапазона вывода: print ... range=начало-конец ---
    size_t range_start = 0, range_end = output_students.size();
    size_t range_pos = fields.find("range=");
    if (range_pos != std::string::npos) {
        size_t eq = range_pos + 6;
        size_t dash = fields.find('-', eq);
        if (dash != std::string::npos) {
            std::string start_str = fields.substr(eq, dash - eq);
            std::string end_str = fields.substr(dash + 1, fields.find_first_of(" ", dash + 1) - (dash + 1));
            try {
                range_start = std::stoul(start_str) - 1;
                range_end = std::stoul(end_str);
                if (range_start > output_students.size()) range_start = output_students.size();
                if (range_end > output_students.size()) range_end = output_students.size();
            }
            catch (...) {}
        }
    }
    for (size_t idx = range_start; idx < range_end; ++idx) {
        const Student& student = students[output_students[idx]];
        std::istringstream iss(fields);
        std::string field;
        int fn = 0;
        while (iss >> field) {
            if (field == "id") ++fn, std::cout << student.id << "\t";
            else if (field == "name") ++fn, std::cout << student.name << "\t";
            else if (field == "group") ++fn, std::cout << student.group << "\t";
            else if (field == "rating") ++fn, std::cout << student.rating << "\t";
            else if (field == "info") ++fn, std::cout << student.info << "\t";
            else break;
        }
        if (!fn) std::cout << student.id << "\t" << student.name << "\t" << student.group << "\t" << student.rating << "\t" << student.info;
        std::cout << "\n";
    }
}
// Редактирование выбранных записей(всех)
void Database::update(const std::string& command) {
    auto criteria = parseCriteria(command);
    for (const auto& crit : criteria) {
        const std::string& field = crit.first;
        const std::string& value = crit.second;
        for (const size_t& i : selectedStudents) {
            if (field == "name") {
                if (!validate_name(value)) {
                    std::cout << "Ошибка: некорректное ФИО (пример: Иванов Иван Иванович)\n";
                    break;
                }
                strncpy(students[i].name, value.c_str(), 63);
                students[i].name[63] = L'\0';
            }
            else if (field == "group") {
                int group = 0;
                try { group = std::stoi(value); }
                catch (...) { break; }
                if (!validate_group(group)) {
                    std::cout << "Ошибка: некорректная группа (целое число > 0)\n";
                    break;
                }
                students[i].group = group;
            }
            else if (field == "rating") {
                double rating = 0;
                try { rating = std::stod(value); }
                catch (...) { break; }
                if (!validate_rating(rating)) {
                    std::cout << "Ошибка: некорректная оценка (от 2 до 5)\n";
                    break;
                }
                students[i].rating = rating;
            }
            else if (field == "info") {
                students[i].info = value;
            }
        }
    }
    sort();
    saveToFile(dbFile);
    studentsBN.clear();
    studentsBG.clear();
    studentsBR.clear();
    selectedStudents.clear();
    selectedStudents.reserve(students.size());
    selectedStudents.resize(students.size());
    for (size_t i = 0; i < students.size(); ++i) {
        studentsBN.insert(Index{ i });
        studentsBG.insert(Index{ i });
        studentsBR.insert(Index{ i });
        selectedStudents[i] = i;
    }
    std::cout << "Отредактированы записи\n";
}
// Удаление среди выбранных записей
void Database::remove() {
    int count = 0;
    for (const size_t& i : selectedStudents) {
        students.erase(students.begin() + i - count);
        count++;
    }
    saveToFile(dbFile);
    // Пересоздаем деревья, т.к. все индексы после удаленных записей сдвинулись, а значит данные в деревьях невалидны
    studentsBN.clear();
    studentsBG.clear();
    studentsBR.clear();
    selectedStudents.clear();
    selectedStudents.reserve(students.size());
    selectedStudents.resize(students.size());
    for (size_t i = 0; i < students.size(); ++i) {
        studentsBN.insert(Index{ i });
        studentsBG.insert(Index{ i });
        studentsBR.insert(Index{ i });
        selectedStudents[i] = i;
    }
    std::cout << "Удалены записи: " << count << "\n";
}
// Добавление записи
void Database::add(const std::string& command) {
    std::istringstream iss(command);
    Student newStudent;
    iss.getline(newStudent.name, 64, L'\t');
    iss >> newStudent.group >> newStudent.rating;
    iss.ignore(1);
    std::getline(iss, newStudent.info);
    std::string name_str(newStudent.name);
    if (!validate_name(name_str)) {
        std::cout << "Ошибка: некорректное ФИО (пример: Иванов Иван Иванович)\n";
        return;
    }
    if (!validate_group(newStudent.group)) {
        std::cout << "Ошибка: некорректная группа (целое число > 0)\n";
        return;
    }
    if (!validate_rating(newStudent.rating)) {
        std::cout << "Ошибка: некорректная оценка (от 2 до 5)\n";
        return;
    }
    newStudent.id = nextId++;
    students.push_back(newStudent);
    sort();
    saveToFile(dbFile);
    studentsBN.clear();
    studentsBG.clear();
    studentsBR.clear();
    selectedStudents.clear();
    selectedStudents.reserve(students.size());
    selectedStudents.resize(students.size());
    for (size_t i = 0; i < students.size(); ++i) {
        studentsBN.insert(Index{ i });
        studentsBG.insert(Index{ i });
        studentsBR.insert(Index{ i });
        selectedStudents[i] = i;
    }
    std::cout << "Добавлен студент: " << newStudent.name << "\n";
}

void Database::sort() {
    std::sort(students.begin(), students.end(), [&](const Student& a, const Student& b) {
        if (a.group != b.group)
            return a.group < b.group;
        if (strcmp(a.name, b.name))
            return strcmp(a.name, b.name) < 0;
        if (a.rating != b.rating)
            return a.rating < b.rating;
        return a.info < b.info;
        });
}
// ------------------- Реализация поддержки оповещений -------------------
size_t Database::getVersion() const { return version; }
void Database::clearCallbacks() { changeCallbacks.clear(); }
void Database::notifyOnChange(std::function<void()> callback) { changeCallbacks.push_back(callback); }
void Database::notifyChanged() {
    ++version;
    for (auto& cb : changeCallbacks) cb();
}
