import sys
import socket
import configparser
import json
import re
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout, QLineEdit, QPushButton,
    QLabel, QTableWidget, QTableWidgetItem, QFrame, QGridLayout, QMessageBox, QDialog, QFormLayout, QDialogButtonBox, QHeaderView, QSpinBox, QStatusBar, QComboBox
)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QKeySequence, QColor

CONFIG_FILE = 'client_config.ini'
STATE_FILE = 'client_state.json'

class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Работа с данными")
        self.current_file = "students.txt"
        self.page = 1
        self.page_size = 100
        self.total_records = 0
        self.last_query = "print all"
        self.selected_id = None
        self.state = {}
        self.load_state()
        self.init_network()
        self.init_ui()
        self.open_file()
        self.start_notification_timer()

    def load_state(self):
        try:
            with open(STATE_FILE, 'r', encoding='utf-8') as f:
                self.state = json.load(f)
                self.current_file = self.state.get('file', "")
                self.page = self.state.get('page', 1)
                self.page_size = self.state.get('page_size', 20)
        except:
            self.state = {}

    def save_state(self):
        self.state['file'] = self.current_file
        self.state['page'] = self.page
        self.state['page_size'] = self.page_size
        with open(STATE_FILE, 'w', encoding='utf-8') as f:
            json.dump(self.state, f, ensure_ascii=False)

    def init_network(self):
        config = configparser.ConfigParser()
        config.read(CONFIG_FILE, encoding='utf-8')
        server_ip = config.get('Network', 'server_ip', fallback='127.0.0.1')
        port = config.getint('Network', 'port', fallback=8080)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_address = (server_ip, port)
        try:
            self.sock.connect(self.server_address)
        except Exception:
            QMessageBox.critical(self, "Ошибка", "Не удалось подключиться к серверу")
            self.sock = None

    def init_ui(self):
        main_layout = QVBoxLayout()
        # 1. Открытие файла
        file_layout = QHBoxLayout()
        self.file_input = QLineEdit(self.current_file)
        self.file_input.setPlaceholderText("Файл базы данных")
        open_btn = QPushButton("Открыть")
        save_btn = QPushButton("Сохранить")
        file_layout.addWidget(self.file_input)
        file_layout.addWidget(open_btn)
        file_layout.addWidget(save_btn)
        main_layout.addLayout(file_layout)
        # --- линия ---
        line1 = QFrame()
        line1.setFrameShape(QFrame.HLine)
        line1.setFrameShadow(QFrame.Sunken)
        main_layout.addWidget(line1)
        # 2. Поиск данных (id, фио, группа, оценка)
        search_group = QHBoxLayout()
        self.search_id = QLineEdit()
        self.search_id.setPlaceholderText("id")
        self.search_fio = QLineEdit()
        self.search_fio.setPlaceholderText("ФИО")
        self.search_group_field = QLineEdit()
        self.search_group_field.setPlaceholderText("Группа")
        self.search_rating = QLineEdit()
        self.search_rating.setPlaceholderText("Оценка")
        search_btn = QPushButton("Поиск")
        reselect_btn = QPushButton("Выбрать среди")
        search_group.addWidget(QLabel("id"))
        search_group.addWidget(self.search_id)
        search_group.addWidget(QLabel("ФИО"))
        search_group.addWidget(self.search_fio)
        search_group.addWidget(QLabel("Группа"))
        search_group.addWidget(self.search_group_field)
        search_group.addWidget(QLabel("Оценка"))
        search_group.addWidget(self.search_rating)
        search_group.addWidget(search_btn)
        search_group.addWidget(reselect_btn)
        main_layout.addLayout(search_group)
        # --- линия ---
        line2 = QFrame()
        line2.setFrameShape(QFrame.HLine)
        line2.setFrameShadow(QFrame.Sunken)
        main_layout.addWidget(line2)
        # 3. Редактирование данных
        edit_group = QHBoxLayout()
        self.fio_input = QLineEdit()
        self.fio_input.setPlaceholderText("ФИО")
        self.group_input = QLineEdit()
        self.group_input.setPlaceholderText("Группа")
        self.grade_input = QLineEdit()
        self.grade_input.setPlaceholderText("Оценка")
        self.info_input = QLineEdit()
        self.info_input.setPlaceholderText("Информация")
        add_btn = QPushButton("Добавить")
        edit_btn = QPushButton("Изменить")
        delete_btn = QPushButton("Удалить")
        clear_btn = QPushButton("Очистить")
        copy_btn = QPushButton("Копировать")
        edit_group.addWidget(QLabel("ФИО"))
        edit_group.addWidget(self.fio_input)
        edit_group.addWidget(QLabel("Группа"))
        edit_group.addWidget(self.group_input)
        edit_group.addWidget(QLabel("Оценка"))
        edit_group.addWidget(self.grade_input)
        edit_group.addWidget(QLabel("Инфо"))
        edit_group.addWidget(self.info_input)
        edit_group.addWidget(add_btn)
        edit_group.addWidget(edit_btn)
        edit_group.addWidget(delete_btn)
        edit_group.addWidget(clear_btn)
        edit_group.addWidget(copy_btn)
        main_layout.addLayout(edit_group)
        # --- линия ---
        line3 = QFrame()
        line3.setFrameShape(QFrame.HLine)
        line3.setFrameShadow(QFrame.Sunken)
        main_layout.addWidget(line3)
        # 4. Таблица, пагинация, статус-бар
        pag_layout = QHBoxLayout()
        self.prev_btn = QPushButton("< Предыдущая")
        self.next_btn = QPushButton("Следующая >")
        self.page_label = QLabel(f"Страница {self.page}")
        self.page_input = QSpinBox()
        self.page_input.setMinimum(1)
        self.page_input.setMaximum(100000)
        self.page_input.setValue(self.page)
        self.page_size_box = QComboBox()
        self.page_size_box.addItems(["10", "20", "50", "100"])
        self.page_size_box.setCurrentText(str(self.page_size))
        pag_layout.addWidget(self.prev_btn)
        pag_layout.addWidget(self.next_btn)
        pag_layout.addWidget(QLabel("Страница:"))
        pag_layout.addWidget(self.page_input)
        pag_layout.addWidget(self.page_label)
        pag_layout.addWidget(QLabel("Размер страницы:"))
        pag_layout.addWidget(self.page_size_box)
        main_layout.addLayout(pag_layout)
        self.table = QTableWidget()
        self.table.setColumnCount(4)
        self.table.setHorizontalHeaderLabels(["ФИО", "Группа", "Оценка", "Информация"])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)
        self.table.verticalHeader().setVisible(True)
        main_layout.addWidget(self.table)
        self.status = QStatusBar()
        main_layout.addWidget(self.status)
        self.setLayout(main_layout)
        self.resize(900, 600)
        # --- Сигналы ---
        open_btn.clicked.connect(self.open_file)
        save_btn.clicked.connect(self.save_file)
        add_btn.clicked.connect(self.add_record)
        edit_btn.clicked.connect(self.edit_record)
        delete_btn.clicked.connect(self.delete_record)
        clear_btn.clicked.connect(self.clear_fields)
        copy_btn.clicked.connect(self.copy_fields)
        search_btn.clicked.connect(self.select)
        reselect_btn.clicked.connect(self.select_among)
        self.prev_btn.clicked.connect(self.prev_page)
        self.next_btn.clicked.connect(self.next_page)
        self.page_input.valueChanged.connect(self.goto_page)
        self.page_size_box.currentTextChanged.connect(self.change_page_size)
        self.table.itemSelectionChanged.connect(self.on_row_select)
        # --- Горячие клавиши ---
        self.shortcut_save = QKeySequence(Qt.CTRL + Qt.Key_S)
        self.shortcut_refresh = QKeySequence(Qt.Key_F5)
        # --- Таймер для сброса цвета статуса ---
        self.status_timer = QTimer()
        self.status_timer.setSingleShot(True)
        self.status_timer.timeout.connect(self.reset_status_color)

        self.file_input.setReadOnly(False)
        self.search_id.setReadOnly(False)
        self.search_fio.setReadOnly(False)
        self.search_group_field.setReadOnly(False)
        self.search_rating.setReadOnly(False)
        self.fio_input.setReadOnly(False)
        self.group_input.setReadOnly(False)
        self.grade_input.setReadOnly(False)
        self.info_input.setReadOnly(False)

    def keyPressEvent(self, event):
        if event.matches(QKeySequence.Save):
            self.save_file()
        elif event.key() == Qt.Key_F5:
            self.load_data()
        else:
            super().keyPressEvent(event)

    def set_status(self, text, ok=True):
        self.status.showMessage(text)
        pal = self.status.palette()
        color = QColor('#4CAF50') if ok else QColor('#F44336')
        pal.setColor(self.status.backgroundRole(), color)
        self.status.setStyleSheet(f"background-color: {color.name()}; color: white;")
        self.status_timer.start(2000)

    def reset_status_color(self):
        self.status.setStyleSheet("")

    def send_command(self, command):
        if not self.sock:
            self.set_status("Нет соединения с сервером", False)
            return None
        try:
            cmd_bytes = (command).encode('utf-8')
            cmd_length = len(cmd_bytes)
            self.sock.sendall(cmd_length.to_bytes(4, byteorder='little'))
            self.sock.sendall(cmd_bytes)
            length_bytes = self.sock.recv(4)
            if len(length_bytes) < 4:
                raise ConnectionError("Не удалось получить длину ответа")
            response_length = int.from_bytes(length_bytes, byteorder='little')
            response = b''
            while len(response) < response_length:
                chunk = self.sock.recv(min(65536, response_length - len(response)))
                if not chunk:
                    raise ConnectionError("Соединение прервано во время получения данных")
                response += chunk
            if command.find("print") is None:
                print(response.decode('utf-8').strip())
            return response.decode('utf-8').strip()
        except Exception as e:
            self.set_status(f"Ошибка связи с сервером: {str(e)}", False)
            return None

    def open_file(self):
        fname = self.file_input.text().strip()
        if fname:
            self.current_file = fname
            resp = self.send_command(f"open {fname}")
            if resp:
                self.set_status(resp, 'успешно' in resp.lower())
            self.save_state()
            self.load_data()

    def save_file(self):
        resp = self.send_command("save")
        if resp:
            self.set_status(resp, 'успешно' in resp.lower())

    def load_data(self):
        self.last_query = f"print range={((self.page-1)*self.page_size+1)}-{self.page*self.page_size}"
        resp = self.send_command(self.last_query)
        if resp and 'Нет выбранных записей' not in resp:
            self.update_table(resp)
            self.set_status("Данные загружены", True)
        else:
            self.table.setRowCount(0)
            self.set_status(resp or "Нет данных", False)
        self.save_state()

    def select(self):
        id_val = self.search_id.text().strip()
        fio = self.search_fio.text().strip()
        group = self.search_group_field.text().strip()
        grade = self.search_rating.text().strip()
        query = []
        if id_val:
            query.append(f'id={id_val}')
        if fio:
            query.append(f'name="{fio}"')
        if group:
            query.append(f'group={group}')
        if grade:
            query.append(f'rating={grade}')
        if query:
            cmd = f"select {' '.join(query)}"
            resp = self.send_command(cmd)
            if resp:
                self.set_status(resp, 'успешно' in resp.lower())
            self.load_data()
        else:
            resp = self.send_command("select")
            if resp:
                self.set_status(resp, 'успешно' in resp.lower())
            self.load_data()

    def select_among(self):
        id_val = self.search_id.text().strip()
        fio = self.search_fio.text().strip()
        group = self.search_group_field.text().strip()
        grade = self.search_rating.text().strip()
        query = []
        if id_val:
            query.append(f'id={id_val}')
        if fio:
            query.append(f'name="{fio}"')
        if group:
            query.append(f'group={group}')
        if grade:
            query.append(f'rating={grade}')
        if query:
            cmd = f"reselect {' '.join(query)}"
            resp = self.send_command(cmd)
            if resp:
                self.set_status(resp, 'успешно' in resp.lower())
            self.load_data()
        else:
            resp = self.send_command("select")
            if resp:
                self.set_status(resp, 'успешно' in resp.lower())
            self.load_data()

    def add_record(self):
        fio = self.fio_input.text().strip()
        group = self.group_input.text().strip()
        rating = self.grade_input.text().strip()
        info = self.info_input.text().strip()
        record = f'{fio}\t{group}\t{rating}\t{info}'
        resp = self.send_command(f"add {record}")
        if resp:
            self.set_status(resp, 'успешно' in resp.lower())
        self.load_data()
        self.clear_fields()

    def edit_record(self):
        fio = self.fio_input.text().strip()
        group = self.group_input.text().strip()
        rating = self.grade_input.text().strip()
        info = self.info_input.text().strip()
        params = []
        if fio:
            params.append(f'name="{fio}"')
        if group:
            params.append(f'group={group}')
        if rating:
            params.append(f'rating={rating}')
        if info:
            params.append(f'info="{info}"')
        if params:
            record = ' '.join(params)
            resp = self.send_command(f"update {record}")
            if resp:
                self.set_status(resp, 'успешно' in resp.lower())
            self.load_data()
            self.clear_fields()
        else:
            self.set_status('Вы должны указать изменения', False)

    def delete_record(self):
        resp = self.send_command(f"remove")
        if resp:
            self.set_status(resp, 'успешно' in resp.lower())
        self.load_data()
        self.clear_fields()

    def update_table(self, data):
        lines = data.strip().split('\n')
        self.table.setRowCount(0)
        for line in lines:
            if not line.strip():
                continue
            row = line.split('\t')
            if len(row) < 5:
                continue
            row_idx = self.table.rowCount()
            self.table.insertRow(row_idx)
            # id теперь в vertical header
            self.table.setVerticalHeaderItem(row_idx, QTableWidgetItem(row[0]))
            for col_idx, value in enumerate(row[1:]):
                item = QTableWidgetItem(value)
                item.setToolTip(value)
                self.table.setItem(row_idx, col_idx, item)
        self.page_label.setText(f"Страница {self.page}")
        self.page_input.setValue(self.page)
        self.selected_id = None

    def on_row_select(self):
        items = self.table.selectedItems()
        if not items or len(items) < 4:
            self.selected_id = None
            return
        row = self.table.currentRow()
        id_val = self.table.verticalHeaderItem(row).text() if self.table.verticalHeaderItem(row) else None
        self.selected_id = id_val
        self.fio_input.setText(items[0].text())
        self.group_input.setText(items[1].text())
        self.grade_input.setText(items[2].text())
        self.info_input.setText(items[3].text())

    def clear_fields(self):
        self.fio_input.clear()
        self.group_input.clear()
        self.grade_input.clear()
        self.info_input.clear()
        self.table.clearSelection()
        self.selected_id = None

    def copy_fields(self):
        fio = self.fio_input.text()
        group = self.group_input.text()
        rating = self.grade_input.text()
        info = self.info_input.text()
        QApplication.clipboard().setText(f"{fio}\t{group}\t{rating}\t{info}")
        self.set_status("Скопировано", True)

    def prev_page(self):
        if self.page > 1:
            self.page -= 1
            self.load_data()

    def next_page(self):
        self.page += 1
        self.load_data()

    def goto_page(self, val):
        self.page = val
        self.load_data()

    def change_page_size(self, val):
        self.page_size = int(val)
        self.page = 1
        self.load_data()

    def closeEvent(self, event):
        if hasattr(self, 'sock') and self.sock:
            self.sock.close()
        self.save_state()
        event.accept()

    def start_notification_timer(self):
        self.notify_timer = QTimer(self)
        self.notify_timer.timeout.connect(self.check_db_update_notification)
        self.notify_timer.start(500)

    def check_db_update_notification(self):
        if not self.sock:
            return
        try:
            import select
            rlist, _, _ = select.select([self.sock], [], [], 0)
            if rlist:
                peek = self.sock.recv(4, socket.MSG_PEEK)
                if len(peek) == 4 and int.from_bytes(peek, 'little', signed=True) == -1:
                    # Считать уведомление
                    self.sock.recv(4)
                    self.show_db_update_dialog()
        except Exception:
            pass

    def show_db_update_dialog(self):
        msg = QMessageBox(self)
        msg.setIcon(QMessageBox.Warning)
        msg.setWindowTitle("Изменение базы данных")
        msg.setText("База данных была изменена другим пользователем. Вы точно хотите продолжить работу с данной версией?")
        yes_btn = msg.addButton("Да", QMessageBox.AcceptRole)
        open_btn = msg.addButton("Открыть новую БД", QMessageBox.ActionRole)
        msg.setDefaultButton(yes_btn)
        msg.exec_()
        if msg.clickedButton() == open_btn:
            self.open_file()
        # Если "Да" — ничего не делаем, пользователь продолжает работу

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
