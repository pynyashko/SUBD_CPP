#include "subd.h"
#include <regex>
#include <typeinfo>

// -------------------------------------------------- Функции перевода строк из разных кодировок --------------------------------------------------
// Конвертация UTF-8 (файл) → UTF-16 (в памяти)
std::wstring utf8_to_utf16(const std::string& input) {

    // Определим размер буфера для результата
    size_t size = std::mbstowcs(nullptr, input.c_str(), 0);
    if (size == static_cast<size_t>(-1)) {
        throw std::runtime_error("Conversion to wide string failed.");
    }

    std::wstring result(size, L'\0');
    std::mbstowcs(&result[0], input.c_str(), size);
    return result;
}

// Конвертация UTF-16 → UTF-8 (для имени файла)
std::string utf16_to_utf8(const std::wstring& input) {

    // Определим размер необходимого буфера
    size_t size = std::wcstombs(nullptr, input.c_str(), 0);
    if (size == static_cast<size_t>(-1)) {
        throw std::runtime_error("Conversion to narrow string failed.");
    }

    std::string result(size, '\0');
    std::wcstombs(&result[0], input.c_str(), size);
    return result;
}

// -------------------------------------------------- Компараторы для деревьев --------------------------------------------------
// ------------------- Реализация CompareByName -------------------
bool Database::CompareByName::operator()(Index a, const wchar_t* b) const {
    size_t len = wcslen(b) - 1;
    if (b[len] == L'*')
        return wcsncmp((*students_ptr)[(size_t)a].name, b, len) < 0;
    else
        return wcscmp((*students_ptr)[(size_t)a].name, b) < 0;
}
bool Database::CompareByName::operator()(const wchar_t* a, Index b) const {
    size_t len = wcslen(a) - 1;
    if (a[len] == L'*')
        return wcsncmp(a, (*students_ptr)[(size_t)b].name, len) < 0;
    else
        return wcscmp(a, (*students_ptr)[(size_t)b].name) < 0;
}
bool Database::CompareByName::operator()(Index a, Index b) const {
    int res = wcscmp((*students_ptr)[(size_t)a].name,
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
bool validate_name(const std::wstring& name) {
    std::wregex re(LR"(^[А-ЯЁ][а-яё]+ [А-ЯЁ][а-яё]+ [А-ЯЁ][а-яё]+$)");
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
void Database::loadFromFile(const std::wstring& filename) {
    // Открываем файл для чтения в UTF-8
    std::ifstream file(utf16_to_utf8(filename));
    if (!file.is_open()) {
        std::wcout << L"Ошибка: не удалось открыть файл " << filename << L"\n";
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
        // Конвертируем строку UTF-8 в wstring (UTF-16)
        std::wstring wline = utf8_to_utf16(line);
        std::wistringstream iss(wline);
        std::wstring id_str, name;
        std::getline(iss, id_str, L'\t');
        bool has_id = std::all_of(id_str.begin(), id_str.end(), ::iswdigit);
        if (has_id) {
            temp.id = std::stoi(id_str);
            std::getline(iss, name, L'\t');
        }
        else {
            temp.id = nextId++;
            name = id_str;
        }
        // Копируем имя в temp.name, учитывая максимальную длину 64
        wcsncpy(temp.name, name.c_str(), 63);
        temp.name[63] = L'\0'; // Гарантируем завершающий нуль

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
    std::wcout << L"База данных загружена из " << filename << L"(" << students.size() << L")\n";
}
// Сохранение БД в файл
void Database::saveToFile(const std::wstring& filename) {
    // Открываем файл для записи в UTF-8
    std::ofstream file(utf16_to_utf8(filename));
    file.imbue(std::locale(file.getloc(), new NoSeparator));
    if (!file.is_open()) {
        std::wcout << L"Ошибка: не удалось сохранить файл " << filename << L"\n";
        return;
    }

    for (const auto& student : students) {
        std::wstring name_wstr(student.name);
        std::string name_utf8 = utf16_to_utf8(name_wstr);
        std::string info_utf8 = utf16_to_utf8(student.info);
        file << student.id << "\t" << name_utf8 << "\t" << student.group << "\t" << student.rating << "\t" << info_utf8 << "\n";
    }

    file.close();
    notifyChanged();
}
// Парсинг критериев из команды (строка "name=Кузьмин* group=101-103" разобьется на пары ключ-значение: [field]: value (["name"]: "Кузьмин*", ["group"]: "101-103"))
std::map<std::wstring, std::wstring> Database::parseCriteria(const std::wstring& command) const {
    std::map<std::wstring, std::wstring> result;

    // Регулярное выражение для поиска пар ключ=значение
    std::wregex pattern(LR"((\w+)=("[^"]*"|[^\s]*))");
    std::wsregex_iterator it(command.begin(), command.end(), pattern);
    std::wsregex_iterator end;

    while (it != end) {
        std::wsmatch match = *it;
        std::wstring key = match[1].str();
        std::wstring value = match[2].str();

        // Удаляем кавычки, если они есть
        if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') {
            value = value.substr(1, value.size() - 2);
        }

        result[key] = value;
        ++it;
    }

    return result;
}
// Проверка соответствия записи критериям(тем самым парам ключ-значение)
bool Database::matchesCriteria(const Student& student, const std::map<std::wstring, std::wstring>& criteria) const {
    for (const auto& crit : criteria) {
        const std::wstring& field = crit.first;
        const std::wstring& value = crit.second;

        if (value == L"*")
            continue;

        if (field == L"id") {
            int id = student.id;
            size_t dashPos = value.find(L'-');
            if (dashPos != std::wstring::npos) {
                std::wstring startStr = value.substr(0, dashPos);
                std::wstring endStr = value.substr(dashPos + 1);
                int start = startStr == L"*" ? std::numeric_limits<int>::min() : std::stoi(startStr);
                int end = endStr == L"*" ? std::numeric_limits<int>::max() : std::stoi(endStr);
                if (id < start || id > end) {
                    return false;
                }
            }
            else {
                if (std::to_wstring(id) != value) {
                    return false;
                }
            }
        }
        else if (field == L"name") {
            // Заменяем * в критерии на .* (в regex это "любое количество любых символов")
            std::wstring regexPattern = L"^"; // ^ - начало строки
            size_t lastPos = 0;
            size_t starPos = value.find(L'*');

            // Разбиваем критерий на части и заменяем * на .*
            while (starPos != std::string::npos) {
                regexPattern += value.substr(lastPos, starPos - lastPos) + L".*";
                lastPos = starPos + 1;
                starPos = value.find(L'*', lastPos);
            }
            regexPattern += value.substr(lastPos) + L"$"; // $ - конец строки

            // Создаём regex и проверяем совпадение
            std::wregex re(regexPattern);
            if (!std::regex_search(student.name, re)) {
                return false;
            }
        }
        else if (field == L"group") {
            int group = student.group;
            size_t dashPos = value.find(L'-');
            if (dashPos != std::wstring::npos) {
                std::wstring startStr = value.substr(0, dashPos);
                std::wstring endStr = value.substr(dashPos + 1);
                int start = startStr == L"*" ? std::numeric_limits<int>::min() : std::stoi(startStr);
                int end = endStr == L"*" ? std::numeric_limits<int>::max() : std::stoi(endStr);
                if (group < start || group > end) {
                    return false;
                }
            }
            else {
                if (std::to_wstring(group) != value) {
                    return false;
                }
            }
        }
        else if (field == L"rating") {
            double rating = student.rating;
            size_t dashPos = value.find(L'-');
            if (dashPos != std::string::npos) {
                std::wstring startStr = value.substr(0, dashPos);
                std::wstring endStr = value.substr(dashPos + 1);
                double start = startStr == L"*" ? -std::numeric_limits<double>::max() : std::stod(startStr);
                double end = endStr == L"*" ? std::numeric_limits<double>::max() : std::stod(endStr);
                if (rating < start || rating > end) {
                    return false;
                }
            }
            else {
                if (std::to_wstring(rating) != value) {
                    return false;
                }
            }
        }
    }
    return true;
}

// -------------------------------------------------- Внешние методы работы с БД --------------------------------------------------
// Выполнение команды из строки
void Database::parseCommand(const std::wstring& full_command) {
    std::wstring command;
    std::wstring args;
    if (size_t space = full_command.find(L" "); space == std::string::npos) {
        command = full_command;
        args = L"";
        if (command == L"open" ||
            command == L"add" ||
            command == L"update") {
            std::wcout << "Не удалось обработать команду\n";
            return;
        }
    }
    else {
        command = full_command.substr(0, space);
        args = full_command.substr(space + 1, full_command.length() - space - 1);
    }
    if (command == L"open") {
        selectDB(args);
    }
    else if (command == L"save") {
        saveDB();
    }
    else if (command == L"select") {
        select(args);
    }
    else if (command == L"reselect") {
        reselect(args);
    }
    else if (command == L"print") {
        print(args);
    }
    else if (command == L"add") {
        add(args);
    }
    else if (command == L"remove") {
        remove();
    }
    else if (command == L"update") {
        update(args);
    }
    else {
        std::wcout << L"Не удалось обработать команду\n";
    }
}
// -------------------------------------------------- Работа с файлом БД --------------------------------------------------
// Выбор файла базы данных
void Database::selectDB(const std::wstring& filename) {
    dbFile = filename;
    loadFromFile(dbFile);
    notifyChanged();
}
// Сохранение базы данных
void Database::saveDB() {
    saveToFile(dbFile);
    std::wcout << L"База данных сохранена в " << dbFile << L"\n";
}
// -------------------------------------------------- Выборка из данных --------------------------------------------------
// Выборка записей
void Database::select(const std::wstring& command) {
    auto criteria = parseCriteria(command);
    if (criteria.empty()) {
        selectedStudents.clear();
        selectedStudents.reserve(students.size());
        selectedStudents.resize(students.size());
        for (size_t i = 0; i < students.size(); ++i) {
            selectedStudents[i] = i;
        }
        std::wcout << L"Выбрано " << selectedStudents.size() << L" записей после выборки\n";
        return;
    }
    selectedStudents.clear();

    // --- Быстрый поиск по деревьям ---
    std::set<Index>::iterator startN, endN, startG, endG, startR, endR; // Диапазоны валидных записей по деревьям
    bool N{}, G{}, R{}; // Были ли найдены записи по этим деревьям
    std::wstring id_criteria;
    for (const auto& crit : criteria) {
        const std::wstring& field = crit.first;
        const std::wstring& value = crit.second;
        if (field == L"id") {
            id_criteria = value;
        }
        // --- Поиск по name ---
        else if (field == L"name") {
            if (value == L"*") continue;
            size_t dashPos = value.find(L'-');
            if (dashPos != std::string::npos) { // Если у нас поиск по диапазону значений (name=А*-Б*, ...)
                std::wstring startStr = value.substr(0, dashPos);
                std::wstring endStr = value.substr(dashPos + 1);
                if (startStr > endStr)
                    if (startStr == L"*" && endStr == L"*") continue;
                if (size_t starPos = startStr.find(L"*"); starPos != std::string::npos && starPos != startStr.length() - 1) continue;
                if (startStr == L"*") startN = studentsBN.begin();
                else startN = studentsBN.equal_range(startStr.c_str()).first;
                if (size_t starPos = endStr.find(L"*"); starPos != std::string::npos && starPos != endStr.length() - 1) continue;
                if (endStr == L"*") endN = studentsBN.end();
                else endN = studentsBN.equal_range(endStr.c_str()).second;
            }
            else { // Если у нас поиск по одному значению
                if (size_t starPos = value.find(L"*"); starPos != std::string::npos && starPos != value.length() - 1) continue;
                auto [f, s] = studentsBN.equal_range(value.c_str());
                startN = f;
                endN = s;
            }
            N = true;
        }
        // --- Поиск по group ---
        else if (field == L"group") {
            if (value == L"*") continue;
            size_t dashPos = value.find(L'-');
            if (dashPos != std::wstring::npos) { // Если у нас поиск по диапазону значений (group=101-105, ...)
                std::wstring startStr = value.substr(0, dashPos);
                std::wstring endStr = value.substr(dashPos + 1);
                if (startStr == L"*" && endStr == L"*") continue;
                if (startStr == L"*") startG = studentsBG.begin();
                else startG = studentsBG.equal_range(std::stoi(startStr)).first;
                if (endStr == L"*") endG = studentsBG.end();
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
        else if (field == L"rating") {
            if (value == L"*") continue;
            size_t dashPos = value.find(L'-');
            if (dashPos != std::string::npos) { // Если у нас поиск по диапазону значений (rating=3-5, ...)
                std::wstring startStr = value.substr(0, dashPos);
                std::wstring endStr = value.substr(dashPos + 1);
                if (startStr == L"*" && endStr == L"*") continue;
                if (startStr == L"*") startR = studentsBR.begin();
                else startR = studentsBR.equal_range(std::stod(startStr)).first;
                if (endStr == L"*") endR = studentsBR.end();
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
        std::wcout << L"Выбрано " << selectedStudents.size() << L" записей после выборки\n";
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
            std::map<std::wstring, std::wstring> id_map = { {L"id", id_criteria} };
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
            std::map<std::wstring, std::wstring> id_map = { {L"id", id_criteria} };
            if (matchesCriteria(students[i], id_map)) selectedStudents.push_back(i);
        }
    }
    std::wcout << L"Выбрано " << selectedStudents.size() << L" записей после выборки\n";
}
// Повторная выборка
void Database::reselect(const std::wstring& command) {
    if (selectedStudents.empty()) {
        std::wcout << L"Нет выбранных записей для повторной выборки\n";
        return;
    }
    auto criteria = parseCriteria(command);
    if (criteria.empty()) {
        std::wcout << L"Выбрано " << selectedStudents.size() << L" записей после повторной выборки\n";
        return;
    }
    std::vector<size_t> temp;
    for (size_t i = 0; i < selectedStudents.size(); ++i) {
        if (matchesCriteria(students[selectedStudents[i]], criteria)) {
            temp.push_back(selectedStudents[i]);
        }
    }
    selectedStudents = temp;
    std::wcout << L"Выбрано " << selectedStudents.size() << L" записей после повторной выборки\n";
}
// Вывод выбранных записей
void Database::print(const std::wstring& fields) const {
    std::vector<size_t> output_students = selectedStudents;
    std::wstring sort_value;
    size_t sort_pos = fields.find(L"sort");
    if (sort_pos != std::wstring::npos) {
        sort_value = fields.substr(sort_pos + 4);
        sort_value.erase(0, sort_value.find_first_not_of(L" "));
        sort_value.erase(sort_value.find_last_not_of(L" ") + 1);
    }
    if (!sort_value.empty()) {
        std::sort(output_students.begin(), output_students.end(), [&](size_t a, size_t b) {
            if (sort_value == L"name")
                return wcscmp(students[a].name, students[b].name) <= 0;
            else if (sort_value == L"group")
                return students[a].group < students[b].group;
            else if (sort_value == L"rating")
                return students[a].rating < students[b].rating;
            else
                return wcscmp(students[a].name, students[b].name) <= 0;
            });
    }
    // --- Поддержка диапазона вывода: print ... range=начало-конец ---
    size_t range_start = 0, range_end = output_students.size();
    size_t range_pos = fields.find(L"range=");
    if (range_pos != std::wstring::npos) {
        size_t eq = range_pos + 6;
        size_t dash = fields.find(L'-', eq);
        if (dash != std::wstring::npos) {
            std::wstring start_str = fields.substr(eq, dash - eq);
            std::wstring end_str = fields.substr(dash + 1, fields.find_first_of(L" ", dash + 1) - (dash + 1));
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
        std::wistringstream iss(fields);
        std::wstring field;
        int fn = 0;
        while (iss >> field) {
            if (field == L"id") ++fn, std::wcout << student.id << L"\t";
            else if (field == L"name") ++fn, std::wcout << student.name << L"\t";
            else if (field == L"group") ++fn, std::wcout << student.group << L"\t";
            else if (field == L"rating") ++fn, std::wcout << student.rating << L"\t";
            else if (field == L"info") ++fn, std::wcout << student.info << L"\t";
            else break;
        }
        if (!fn) std::wcout << student.id << L"\t" << student.name << L"\t" << student.group << L"\t" << student.rating << L"\t" << student.info;
        std::wcout << L"\n";
    }
}
// Редактирование выбранных записей(всех)
void Database::update(const std::wstring& command) {
    auto criteria = parseCriteria(command);
    for (const auto& crit : criteria) {
        const std::wstring& field = crit.first;
        const std::wstring& value = crit.second;
        for (const size_t& i : selectedStudents) {
            if (field == L"name") {
                if (!validate_name(value)) {
                    std::wcout << L"Ошибка: некорректное ФИО (пример: Иванов Иван Иванович)\n";
                    break;
                }
                wcsncpy(students[i].name, value.c_str(), 63);
                students[i].name[63] = L'\0';
            }
            else if (field == L"group") {
                int group = 0;
                try { group = std::stoi(value); }
                catch (...) { break; }
                if (!validate_group(group)) {
                    std::wcout << L"Ошибка: некорректная группа (целое число > 0)\n";
                    break;
                }
                students[i].group = group;
            }
            else if (field == L"rating") {
                double rating = 0;
                try { rating = std::stod(value); }
                catch (...) { break; }
                if (!validate_rating(rating)) {
                    std::wcout << L"Ошибка: некорректная оценка (от 2 до 5)\n";
                    break;
                }
                students[i].rating = rating;
            }
            else if (field == L"info") {
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
    std::wcout << L"Отредактированы записи\n";
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
    std::wcout << L"Удалены записи: " << count << L"\n";
}
// Добавление записи
void Database::add(const std::wstring& command) {
    std::wistringstream iss(command);
    Student newStudent;
    iss.getline(newStudent.name, 64, L'\t');
    iss >> newStudent.group >> newStudent.rating;
    iss.ignore(1);
    std::getline(iss, newStudent.info);
    std::wstring name_str(newStudent.name);
    if (!validate_name(name_str)) {
        std::wcout << L"Ошибка: некорректное ФИО (пример: Иванов Иван Иванович)\n";
        return;
    }
    if (!validate_group(newStudent.group)) {
        std::wcout << L"Ошибка: некорректная группа (целое число > 0)\n";
        return;
    }
    if (!validate_rating(newStudent.rating)) {
        std::wcout << L"Ошибка: некорректная оценка (от 2 до 5)\n";
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
    std::wcout << L"Добавлен студент: " << newStudent.name << L"\n";
}

void Database::sort() {
    std::sort(students.begin(), students.end(), [&](const Student& a, const Student& b) {
        if (a.group != b.group)
            return a.group < b.group;
        if (wcscmp(a.name, b.name))
            return wcscmp(a.name, b.name) < 0;
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
