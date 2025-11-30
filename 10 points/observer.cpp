#include <iostream>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <sstream>
#include "shared_data.h"

volatile sig_atomic_t stop = 0;

void signal_handler(int signum) {
    stop = 1;
    std::cout << "\nНаблюдатель: Получен сигнал завершения" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (argc < 2) {
        std::cout << "Использование: ./observer <номер_наблюдателя>" << std::endl;
        std::cout << "Пример: ./observer 1, ./observer 2, ./observer 3" << std::endl;
        std::cout << "Можно запустить до 5 наблюдателей одновременно" << std::endl;
        return 1;
    }
    
    int obs_id = atoi(argv[1]);
    if (obs_id < 1 || obs_id > 5) {
        std::cout << "Номер наблюдателя должен быть от 1 до 5" << std::endl;
        return 1;
    }
    
    std::cout << "Наблюдатель #" << obs_id << ": Запуск" << std::endl;
    std::cout << "Подключение к персональному каналу" << std::endl;
    
    std::stringstream fifo_name;
    fifo_name << FIFO_BASE_NAME << obs_id;
    std::string fifo_path = fifo_name.str();
    
    std::cout << "Подключаюсь к: " << fifo_path << std::endl;
    std::cout << "Ожидаю события" << std::endl;
    std::cout << "Нажмите Ctrl+C для досрочного завершения" << std::endl;
    
    int fifo_fd = open(fifo_path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        perror("open FIFO");
        std::cout << "Сначала запустите администратора: ./hotel_admin" << std::endl;
        return 1;
    }
    
    Message msg;
    int msg_count = 0;
    time_t start_time = time(nullptr);
    const int MAX_WORK_TIME = 360;
    
    int arrived = 0, served = 0, rejected = 0, 
        departed = 0, status_count = 0, waiting_count = 0;
    
    while (!stop && (time(nullptr) - start_time) < MAX_WORK_TIME) {
        ssize_t bytes = read(fifo_fd, &msg, sizeof(Message));
        
        if (bytes > 0) {
            msg_count++;
            std::string gender_str = (msg.gender == LADY) ? "Леди" : "Джентльмен";
            
            std::string arrived_verb = (msg.gender == LADY) ? "прибыла" : "прибыл";
            std::string served_verb = (msg.gender == LADY) ? "размещена" : "размещен";
            std::string rejected_verb = (msg.gender == LADY) ? "отказано" : "отказан";
            std::string departed_verb = (msg.gender == LADY) ? "уходит" : "уходит";
            
            time_t now = time(0);
            std::string timestamp = ctime(&now);
            timestamp.pop_back();
            
            switch (msg.type) {
                case MSG_CLIENT_ARRIVED:
                    std::cout << "Наблюдатель #" << obs_id 
                              << ": " << gender_str << " #" << msg.client_id << " " << arrived_verb << " в гостиницу" << std::endl;
                    arrived++;
                    break;
                    
                case MSG_CLIENT_SERVED:
                    std::cout << "Наблюдатель #" << obs_id << ": " << gender_str << " #" << msg.client_id << " " << served_verb << ": " << msg.details << std::endl;
                    served++;
                    break;
                    
                case MSG_CLIENT_WAITING:
                    std::cout << "Наблюдатель #" << obs_id << ": " << gender_str << " #" << msg.client_id << " ждет соседа для двухместного номера" << std::endl;
                    waiting_count++;
                    break;
                    
                case MSG_CLIENT_REJECTED:
                    std::cout << "Наблюдатель #" << obs_id 
                              << ": " << gender_str << " #" << msg.client_id << " " << rejected_verb << ": " << msg.details << std::endl;
                    rejected++;
                    break;
                    
                case MSG_CLIENT_DEPARTED:
                    std::cout << "Наблюдатель #" << obs_id 
                              << ": " << gender_str << " #" << msg.client_id << " " << departed_verb << ": " << msg.details << std::endl;
                    departed++;
                    break;
                    
                case MSG_STATUS_UPDATE:
                    std::cout << "Наблюдатель #" << obs_id << ": Статус гостиницы - " << msg.single_rooms << " одноместных, "
                              << msg.double_rooms << " двухместных, " << msg.waiting_ladies << " леди ждут, " << msg.waiting_gentlemen << " джентльменов ждут, "
                              << msg.total_served << " обслужено, " << msg.total_rejected << " отказано" << " (" << msg.details << ")" << std::endl;
                    status_count++;
                    break;
                    
                default:
                    std::cout << "Наблюдатель #" << obs_id  << ": Неизвестный тип сообщения" << std::endl;
                    break;
            }
            
            if (msg_count % 15 == 0) {
                std::cout << "Наблюдатель #" << obs_id << ": Статистика - " 
                          << "прибыло: " << arrived 
                          << ", размещено: " << served
                          << ", ждут: " << waiting_count
                          << ", отказано: " << rejected
                          << ", ушло: " << departed
                          << ", статусов: " << status_count << std::endl;
            }
        } else if (bytes == 0) {
            close(fifo_fd);
            fifo_fd = open(fifo_path.c_str(), O_RDONLY | O_NONBLOCK);
            if (fifo_fd == -1) {
                sleep(1);
            }
        }
        
        sleep(1);
    
    }
    
    int total_seconds = time(nullptr) - start_time;
    if (total_seconds >= MAX_WORK_TIME) {
        std::cout << "\nНаблюдатель #" << obs_id << ": Время работы истекло" << std::endl;
    }
    
    if (fifo_fd != -1) {
        close(fifo_fd);
    }
    
    std::cout << "\nНаблюдатель #" << obs_id << ": Финальная статистика" << std::endl;
    std::cout << "Всего сообщений: " << msg_count << std::endl;
    std::cout << "Прибыло клиентов: " << arrived << std::endl;
    std::cout << "Размещено клиентов: " << served << std::endl;
    std::cout << "Ожидало клиентов: " << waiting_count << std::endl;
    std::cout << "Отказано клиентов: " << rejected << std::endl;
    std::cout << "Ушло клиентов: " << departed << std::endl;
    std::cout << "Время работы: " << total_seconds << " секунд" << std::endl;
    
    if (arrived > 0) {
        int served_percent = (served * 100) / arrived;
        int rejected_percent = (rejected * 100) / arrived;
        std::cout << "Процент отказов: " << rejected_percent << "%" << std::endl;
    }
    
    if (waiting_count > 0) {
        std::cout << "Клиентов ожидало соседа: " << waiting_count << std::endl;
    }
    
    std::cout << "Наблюдатель #" << obs_id << ": Завершено" << std::endl;
    
    return 0;
}