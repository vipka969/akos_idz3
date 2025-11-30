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
#include "shared_data.h"

volatile sig_atomic_t stop = 0;
SharedData* data = nullptr;
int shm_fd = -1;
sem_t* sem_admin = nullptr;
sem_t* sem_output = nullptr;

void signal_handler(int signum) {
    (void)signum;
    stop = 1;
    std::cout << "\nАдминистратор: Получен сигнал завершения" << std::endl;
}

void cleanup() {
    std::cout << "\nАдминистратор: Очистка ресурсов" << std::endl;
    
    if (data) {
        data->hotel_active = false;
    }
    
    sleep(2);
    
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

void print_status() {
    sem_wait(sem_output);
    std::cout << "\nСтатус гостиницы" << std::endl;
    std::cout << "Одноместных номеров: " << data->single_rooms << "/7" << std::endl;
    std::cout << "Двухместных номеров: " << data->double_rooms << "/10" << std::endl;
    
    int ladies_waiting_count = 0;
    int gentlemen_waiting_count = 0;
    for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
        if (data->waiting_ladies[i].pid != 0 && !data->waiting_ladies[i].is_matched) {
            ladies_waiting_count++;
        }
        if (data->waiting_gentlemen[i].pid != 0 && !data->waiting_gentlemen[i].is_matched) {
            gentlemen_waiting_count++;
        }
    }
    
    std::cout << "Леди в ожидании: " << ladies_waiting_count << std::endl;
    std::cout << "Джентльменов в ожидании: " << gentlemen_waiting_count << std::endl;
    std::cout << "Обслужено клиентов: " << data->served_clients << std::endl;
    std::cout << "Отказано клиентов: " << data->rejected_clients << std::endl;
    
    sem_post(sem_output);
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    atexit(cleanup);
    
    std::cout << "Администратор: Запуск программы на 7-8 баллов" << std::endl;
    std::cout << "Используются именованные семафоры" << std::endl;
    
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
    
    std::cout << "Администратор: Ресурсы инициализированы" << std::endl;
    std::cout << "Одноместных номеров: " << data->single_rooms << std::endl;
    std::cout << "Двухместных номеров: " << data->double_rooms << std::endl;
    std::cout << "Ожидание клиентов" << std::endl;
    std::cout << "Запустите клиентов в другом терминале:" << std::endl;
    std::cout << "./hotel_client - один клиент" << std::endl;
    std::cout << "./hotel_client 20 - 20 клиентов" << std::endl;
    std::cout << " /hotel_client & ...- несколько независимых процессов" << std::endl;
    std::cout << "Нажмите Ctrl+C для завершения работы" << std::endl;
    
    int cycles = 0;
    const int MAX_CYCLES = 36;
    
    while (!stop && cycles < MAX_CYCLES) {
        sleep(5);
        
        print_status();
        
        cycles++;
    }
    
    if (cycles >= MAX_CYCLES) {
        std::cout << "\nАдминистратор: Время работы истекло (3 минуты)" << std::endl;
    }
    
    std::cout << "\nАдминистратор: Работа завершена!" << std::endl;
    std::cout << "Итоги:" << std::endl;
    std::cout << "Обслужено: " << data->served_clients << " клиентов" << std::endl;
    std::cout << "Отказано: " << data->rejected_clients << " клиентов" << std::endl;
    
    return 0;
}