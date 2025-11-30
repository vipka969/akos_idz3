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
#include <algorithm>
#include "shared_data.h"

volatile sig_atomic_t stop = 0;
SharedData* data = nullptr;
int shm_fd = -1;
sem_t* sem_admin = nullptr;
sem_t* sem_output = nullptr;
int obs_fifo_fd = -1;

void sigpipe_handler(int signum) {
    (void)signum;
}

void signal_handler(int signum) {
    (void)signum;
    stop = 1;
    std::cout << "\nАдминистратор: Получен сигнал завершения" << std::endl;
}

void send_to_observer(MessageType msg_type, Gender gender, int client_id, const std::string& details = "") {
    if (obs_fifo_fd == -1) {
        return;
    }
    
    Message msg = {};
    msg.type = msg_type;
    msg.gender = gender;
    msg.client_id = client_id;
    
    if (msg_type == MSG_STATUS_UPDATE && data) {
        sem_wait(sem_admin);
        msg.single_rooms = data->single_rooms;
        msg.double_rooms = data->double_rooms;
        
        int waiting_ladies = 0;
        int waiting_gentelmen = 0;
        for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
            if (data->waiting_ladies[i].pid != 0 && !data->waiting_ladies[i].is_matched) {
                waiting_ladies++;
            }
            if (data->waiting_gentlemen[i].pid != 0 && !data->waiting_gentlemen[i].is_matched) {
                waiting_gentelmen++;
            }
        }
        msg.waiting_ladies = waiting_ladies;
        msg.waiting_gentlemen = waiting_gentelmen;
        msg.total_served = data->served_clients;
        msg.total_rejected = data->rejected_clients;
        sem_post(sem_admin);
    }
    
    strncpy(msg.details, details.c_str(), sizeof(msg.details)-1);
    msg.details[sizeof(msg.details)-1] = '\0';
    
    ssize_t bytes = write(obs_fifo_fd, &msg, sizeof(Message));
    if (bytes == -1) {
        if (errno == EPIPE) {
            std::cout << "Администратор: Наблюдатель отключился" << std::endl;
            close(obs_fifo_fd);
            obs_fifo_fd = -1;
        }
    }
}

void cleanup() {
    std::cout << "\nАдминистратор: Очистка ресурсов" << std::endl;
    
    if (data) {
        data->hotel_active = false;
    }
    
    sleep(2);
    
    if (obs_fifo_fd != -1) {
        close(obs_fifo_fd);
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
    
    unlink(FIFO_NAME);
    
    std::cout << "Администратор: Ресурсы освобождены" << std::endl;
}

void print_status() {
    if (sem_trywait(sem_output) == 0) {
        std::cout << "\nСтатус гостиницы" << std::endl;
        std::cout << "Одноместных номеров: " << data->single_rooms << "/7" << std::endl;
        std::cout << "Двухместных номеров: " << data->double_rooms << "/10" << std::endl;
        
        int waiting_ladies = 0;
        int waiting_gentelmen = 0;
        for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
            if (data->waiting_ladies[i].pid != 0 && !data->waiting_ladies[i].is_matched) {
                waiting_ladies++;
            }
            if (data->waiting_gentlemen[i].pid != 0 && !data->waiting_gentlemen[i].is_matched) {
                waiting_gentelmen++;
            }
        }
        
        std::cout << "Леди в ожидании: " << waiting_ladies << std::endl;
        std::cout << "Джентльменов в ожидании: " << waiting_gentelmen << std::endl;
        std::cout << "Обслужено клиентов: " << data->served_clients << std::endl;
        std::cout << "Отказано клиентов: " << data->rejected_clients << std::endl;
        
        sem_post(sem_output);
        
        send_to_observer(MSG_STATUS_UPDATE, LADY, 0, "Регулярное обновление статуса");
    } else {
        std::cout << "Администратор: Не удалось получить доступ к выводу" << std::endl;
    }
}

int main() {
    // ДОБАВИТЬ эту строку для игнорирования SIGPIPE
    signal(SIGPIPE, sigpipe_handler);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    atexit(cleanup);
    
    std::cout << "Администратор: Запуск программы на 9 баллов" << std::endl;
    std::cout << "Используются именованные семафоры и наблюдатель" << std::endl;
    
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }
    
    if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return 1;
    }
    
    data = static_cast<SharedData*>(
        mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)
    );
    
    if (data == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return 1;
    }
    
    data->single_rooms = 7;
    data->double_rooms = 10;
    data->served_clients = 0;
    data->rejected_clients = 0;
    data->hotel_active = true;
    
    for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
        data->waiting_ladies[i] = {0, false};
        data->waiting_gentlemen[i] = {0, false};
    }
    
    sem_unlink(SEM_ADMIN_NAME);
    sem_unlink(SEM_OUTPUT_NAME);
    
    sem_admin = sem_open(SEM_ADMIN_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (sem_admin == SEM_FAILED) {
        perror("sem_open admin");
        cleanup();
        return 1;
    }
    
    sem_output = sem_open(SEM_OUTPUT_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (sem_output == SEM_FAILED) {
        perror("sem_open output");
        sem_close(sem_admin);
        sem_unlink(SEM_ADMIN_NAME);
        cleanup();
        return 1;
    }
    
    if (mkfifo(FIFO_NAME, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        cleanup();
        return 1;
    }
    
    obs_fifo_fd = open(FIFO_NAME, O_WRONLY);
    if (obs_fifo_fd == -1) {
        std::cout << "Администратор: Не удалось открыть FIFO для наблюдателя" << std::endl;
    } else {
        std::cout << "Администратор: FIFO для наблюдателя открыт" << std::endl;
    }
    
    std::cout << "Администратор: Ресурсы инициализированы" << std::endl;
    std::cout << "Одноместных номеров: " << data->single_rooms << std::endl;
    std::cout << "Двухместных номеров: " << data->double_rooms << std::endl;
    
    time_t start_time = time(nullptr);
    const int MAX_WORK_TIME = 180;
    
    std::cout << "Ожидание клиентов " << std::endl;
    std::cout << "Запустите в других терминалах:" << std::endl;
    std::cout << "./hotel_client n - n клиентов" << std::endl;
    std::cout << "./observer - наблюдатель" << std::endl;
    std::cout << "Нажмите Ctrl+C для завершения работы" << std::endl;
    
    send_to_observer(MSG_STATUS_UPDATE, LADY, 0, "Гостиница запущена и готова к работе");
    
    int status_count = 0;
    
    while (!stop && (time(nullptr) - start_time) < MAX_WORK_TIME) {
        sleep(5);
        
        print_status();
        status_count++;
    }
    
    int total_seconds = time(nullptr) - start_time;
    if (total_seconds >= MAX_WORK_TIME) {
        std::cout << "\nАдминистратор: Время работы истекло" << std::endl;
    }
    
    std::cout << "\nАдминистратор: Работа завершена!" << std::endl;
    std::cout << "Итоги:" << std::endl;
    std::cout << "Обслужено: " << data->served_clients << " клиентов" << std::endl;
    std::cout << "Отказано: " << data->rejected_clients << " клиентов" << std::endl;
    std::cout << "Время работы: " << total_seconds << " секунд" << std::endl;
    
    send_to_observer(MSG_STATUS_UPDATE, LADY, 0, "Гостиница закрывается - администратор завершает работу");

    sleep(2);
    
    return 0;
}