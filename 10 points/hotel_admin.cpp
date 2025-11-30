#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <csignal>
#include <cstring>
#include <semaphore.h>
#include <errno.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include "shared_data.h"

volatile sig_atomic_t stop = 0;
SharedData* data = nullptr;
int shm_fd = -1;
sem_t* sem_admin = nullptr;
sem_t* sem_output = nullptr;
std::vector<std::string> obs_fifos;

// ДОБАВЛЕНО: Обработчик для SIGPIPE
void sigpipe_handler(int signum) {
    (void)signum;
    // Игнорируем SIGPIPE - просто продолжаем работу
}

void signal_handler(int signum) {
    stop = 1;
    std::cout << "\nАдминистратор: Получен сигнал завершения" << std::endl;
}

void send_to_observers(MessageType msg_type, Gender gender, int client_id, const std::string& details = "") {
    Message msg = {};
    msg.type = msg_type;
    msg.gender = gender;
    msg.client_id = client_id;
    
    if (msg_type == MSG_STATUS_UPDATE && data) {
        sem_wait(sem_admin);
        msg.single_rooms = data->single_rooms;
        msg.double_rooms = data->double_rooms;
        
        int waiting_ladies = 0;
        int waiting_gentlemen = 0;
        for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
            if (data->waiting_ladies[i].pid != 0 && !data->waiting_ladies[i].is_matched) {
                waiting_ladies++;
            }
            if (data->waiting_gentlemen[i].pid != 0 && !data->waiting_gentlemen[i].is_matched) {
                waiting_gentlemen++;
            }
        }
        msg.waiting_ladies = waiting_ladies;
        msg.waiting_gentlemen = waiting_gentlemen;
        msg.total_served = data->served_clients;
        msg.total_rejected = data->rejected_clients;
        sem_post(sem_admin);
    }
    
    strncpy(msg.details, details.c_str(), sizeof(msg.details)-1);
    msg.details[sizeof(msg.details)-1] = '\0';
    
    for (const auto& fifo_path : obs_fifos) {
        int fifo_fd = open(fifo_path.c_str(), O_WRONLY | O_NONBLOCK);
        if (fifo_fd != -1) {
            write(fifo_fd, &msg, sizeof(Message));
            close(fifo_fd);
        }
    }
}

void create_obs_fifos() {
    for (int obs_id = 1; obs_id <= 5; obs_id++) {
        std::stringstream fifo_name;
        fifo_name << FIFO_BASE_NAME << obs_id;
        std::string fifo_path = fifo_name.str();
        
        if (mkfifo(fifo_path.c_str(), 0666) == -1 && errno != EEXIST) {
            std::cout << "Не удалось создать FIFO для наблюдателя " << obs_id << std::endl;
            continue;
        }
        
        obs_fifos.push_back(fifo_path);
        std::cout << "Создан FIFO для наблюдателя #" << obs_id << ": " << fifo_path << std::endl;
    }
}

void cleanup() {
    std::cout << "\nАдминистратор: Очистка ресурсов" << std::endl;
    
    if (data) {
        data->hotel_active = false;
    }
    
    sleep(3);
    
    for (const auto& fifo_path : obs_fifos) {
        unlink(fifo_path.c_str());
        std::cout << "Удален FIFO: " << fifo_path << std::endl;
    }
    
    if (sem_admin) {
        sem_close(sem_admin);
        sem_unlink(SEM_ADMIN_NAME);
    }
    
    if (sem_output) {
        sem_close(sem_output);
        sem_unlink(SEM_OUTPUT_NAME);
    }
    
    if (data) {
        munmap(data, sizeof(SharedData));
    }
    if (shm_fd != -1) {
        close(shm_fd);
        shm_unlink(SHM_NAME);
    }
    
    std::cout << "Администратор: Ресурсы освобождены" << std::endl;
}

void count_waiting(int& ladies, int& gentlemen) {
    ladies = 0;
    gentlemen = 0;
    
    for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
        if (data->waiting_ladies[i].pid != 0 && !data->waiting_ladies[i].is_matched) {
            ladies++;
        }
        if (data->waiting_gentlemen[i].pid != 0 && !data->waiting_gentlemen[i].is_matched) {
            gentlemen++;
        }
    }
}

void print_status() {
    if (sem_trywait(sem_output) == 0) {
        std::cout << "\nСтатус гостиницы" << std::endl;
        std::cout << "Одноместных номеров: " << data->single_rooms << "/7" << std::endl;
        std::cout << "Двухместных номеров: " << data->double_rooms << "/10" << std::endl;
        
        int waiting_ladies, waiting_gentlemen;
        count_waiting(waiting_ladies, waiting_gentlemen);
        
        std::cout << "Леди в ожидании: " << waiting_ladies << std::endl;
        std::cout << "Джентльменов в ожидании: " << waiting_gentlemen << std::endl;
        std::cout << "Обслужено клиентов: " << data->served_clients << std::endl;
        std::cout << "Отказано клиентов: " << data->rejected_clients << std::endl;
        std::cout << "Активных FIFO для наблюдателей: " << obs_fifos.size() << std::endl;
        
        sem_post(sem_output);
        
        send_to_observers(MSG_STATUS_UPDATE, LADY, 0, "Регулярное обновление статуса");
    } else {
        std::cout << "Администратор: Не удалось получить доступ к выводу" << std::endl;
    }
}

int main() {
    // ДОБАВЛЕНО: Обработчик SIGPIPE
    signal(SIGPIPE, sigpipe_handler);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    atexit(cleanup);
    
    std::cout << "Администратор: Запуск программы на 10 баллов" << std::endl;  // Исправлено: "баллов"
    std::cout << "Поддержка нескольких наблюдателей" << std::endl;
    
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd == -1 && errno == EEXIST) {
        std::cout << "Администратор: Гостиница уже запущена!" << std::endl;
        return 1;
    } else if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }
    
    if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        cleanup();
        return 1;
    }
    
    data = static_cast<SharedData*>(
        mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)
    );
    
    if (data == MAP_FAILED) {
        perror("mmap");
        cleanup();
        return 1;
    }
    
    data->single_rooms = 7;
    data->double_rooms = 10;
    data->served_clients = 0;
    data->rejected_clients = 0;
    data->hotel_active = true;
    
    // Исправлено: только 2 поля в структуре
    for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
        data->waiting_ladies[i] = {0, false};
        data->waiting_gentlemen[i] = {0, false};
    }
    
    sem_admin = sem_open(SEM_ADMIN_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (sem_admin == SEM_FAILED) {
        perror("sem_open admin");
        cleanup();
        return 1;
    }
    
    sem_output = sem_open(SEM_OUTPUT_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (sem_output == SEM_FAILED) {
        perror("sem_open output");
        cleanup();
        return 1;
    }
    
    create_obs_fifos();
    
    std::cout << "Администратор: Ресурсы инициализированы" << std::endl;
    std::cout << "Одноместных номеров: " << data->single_rooms << std::endl;
    std::cout << "Двухместных номеров: " << data->double_rooms << std::endl;
    std::cout << "Создано FIFO для наблюдателей: " << obs_fifos.size() << std::endl;
    std::cout << "Ожидание 15 секунд для запуска наблюдателей" << std::endl;
    std::cout << "Запустите наблюдателей в других терминалах:" << std::endl;
    std::cout << "   ./observer 1" << std::endl;
    std::cout << "   ./observer 2" << std::endl;  
    std::cout << "   ./observer 3" << std::endl;
    std::cout << "Затем запустите клиентов: ./hotel_client n" << std::endl;
    std::cout << "Нажмите Ctrl+C для завершения работы" << std::endl;
    
    for (int i = 0; i < 15 && !stop; i++) {
        sleep(1);
        if (i % 5 == 0) {
            std::cout << "Ожидание наблюдателей" << std::endl;
        }
    }
    
    std::cout << "Готов к отправке сообщений " << obs_fifos.size() << " наблюдателям" << std::endl;
    
    send_to_observers(MSG_STATUS_UPDATE, LADY, 0, "Гостиница запущена и готова к работе");
    
    time_t start = time(nullptr);
    const int MAX_WORK_TIME = 300;
    
    while (!stop && (time(nullptr) - start) < MAX_WORK_TIME) {
        sleep(5);
        
        if (!stop) {
            print_status();
        }
    }
    
    int total_seconds = time(nullptr) - start;
    if (total_seconds >= MAX_WORK_TIME) {
        std::cout << "\nАдминистратор: Время работы истекло" << std::endl;
    }
    
    std::cout << "\nАдминистратор: Работа завершена!" << std::endl;
    std::cout << "Итоги:" << std::endl;
    std::cout << "Обслужено: " << data->served_clients << " клиентов" << std::endl;
    std::cout << "Отказано: " << data->rejected_clients << " клиентов" << std::endl;
    std::cout << "Сообщений отправлено " << obs_fifos.size() << " наблюдателям" << std::endl;
    std::cout << "Время работы: " << total_seconds << " секунд" << std::endl;
    
    send_to_observers(MSG_STATUS_UPDATE, LADY, 0, "Гостиница закрывается - администратор завершает работу");
    
    sleep(2);
    
    return 0;
}
