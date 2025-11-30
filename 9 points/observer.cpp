#include <iostream>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <ctime>
#include "shared_data.h"

volatile sig_atomic_t stop = 0;

void signal_handler(int signum) {
    (void)signum;
    stop = 1;
    std::cout << "\nНаблюдатель: Получен сигнал завершения" << std::endl;
}

std::string current_time() {
    time_t now = time(nullptr);
    struct tm* time = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "[%H:%M:%S]", time);
    return std::string(buffer);
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "Наблюдатель: запуск" << std::endl;
    std::cout << "Ожидание сообщений от гостиницы" << std::endl;
    std::cout << "Нажмите Ctrl+C для досрочного завершения" << std::endl;
    
    if (mkfifo(FIFO_NAME, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        return 1;
    }
    
    int fifo_fd = -1;
    Message msg;
    int msg_count = 0;
    time_t start_time = time(nullptr);
    const int MAX_WORK_TIME = 240;
    
    int clients_arrived = 0;
    int clients_served = 0;
    int clients_rejected = 0;
    int clients_departured = 0;
    int status_updates = 0;
    
    std::cout << "Ожидаю события" << std::endl;
    
    while (!stop && (time(nullptr) - start_time) < MAX_WORK_TIME) {
        if (fifo_fd == -1) {
            fifo_fd = open(FIFO_NAME, O_RDONLY | O_NONBLOCK);
            if (fifo_fd == -1) {
                sleep(1);
                continue;
            }
            std::cout << "Наблюдатель: Подключен к гостинице" << std::endl;
        }
        
        ssize_t bytes = read(fifo_fd, &msg, sizeof(Message));
        
        if (bytes > 0) {
            msg_count++;
            std::string gender_str = (msg.gender == LADY) ? "Леди" : "Джентльмен";
            
            switch (msg.type) {
                case MSG_CLIENT_ARRIVED:
                    clients_arrived++;
                    std::cout << " Наблюдатель: " << gender_str << " #" << msg.client_id 
                              << " прибыла в гостиницу" << std::endl;
                    break;
                    
                case MSG_CLIENT_SERVED:
                    clients_served++;
                    std::cout << " Наблюдатель: " << gender_str << " #" << msg.client_id 
                              << " размещена: " << msg.details << std::endl;
                    break;
                    
                case MSG_CLIENT_REJECTED:
                    clients_rejected++;
                    std::cout << " Наблюдатель: " << gender_str << " #" << msg.client_id 
                              << " отказано: " << msg.details << std::endl;
                    break;
                    
                case MSG_CLIENT_DEPARTED:
                    clients_departured++;
                    std::cout << " Наблюдатель: " << gender_str << " #" << msg.client_id 
                              << " уходит: " << msg.details << std::endl;
                    break;
                    
                case MSG_STATUS_UPDATE:
                    status_updates++;
                    std::cout << " Наблюдатель: Статус гостиницы - " 
                              << msg.single_rooms << " одноместных, "
                              << msg.double_rooms << " двухместных, "
                              << msg.waiting_ladies << " леди ждут, "
                              << msg.waiting_gentlemen << " джентльменов ждут, "
                              << msg.total_served << " обслужено, "
                              << msg.total_rejected << " отказано"
                              << " (" << msg.details << ")" << std::endl;
                    break;
                    
                default:
                    std::cout << " Наблюдатель: Неизвестный тип сообщения" << std::endl;
                    break;
            }
        } else if (bytes == 0) {
            close(fifo_fd);
            fifo_fd = -1;
            std::cout << "Наблюдатель: Переподключаюсь к гостинице" << std::endl;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Ошибка чтения из FIFO");
        }
        
        sleep(1);
    }
    
    int total_seconds = time(nullptr) - start_time;
    if (total_seconds >= MAX_WORK_TIME) {
        std::cout << "\nНаблюдатель: Время работы истекло" << std::endl;
    }
    
    if (fifo_fd != -1) {
        close(fifo_fd);
    }
    
    std::cout << "\nНаблюдатель: Завершение работы" << std::endl;
    std::cout << "Статистика наблюдения:" << std::endl;
    std::cout << "Всего сообщений: " << msg_count << std::endl;
    std::cout << "Клиентов прибыло: " << clients_arrived << std::endl;
    std::cout << "Клиентов обслужено: " << clients_served << std::endl;
    std::cout << "Клиентов отказано: " << clients_rejected << std::endl;
    std::cout << "Клиентов ушло: " << clients_departured << std::endl;
    std::cout << "Наблюдатель: Работа завершена" << std::endl;
    return 0;
}