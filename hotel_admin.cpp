#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <csignal>
#include <cstring>
#include "shared_data.h"

volatile sig_atomic_t stop = 0;
SharedData* data = nullptr;
int shm_fd = -1;

void signal_handler(int signum) {
    stop = 1;
    std::cout << "\nПолучен сигнал завершения. Завершаем работу." << std::endl;
}

void cleanup() {
    if (getpid() != getppid()) {
        return;
    }
    
    std::cout << "\nОчистка ресурсов" << std::endl;
    
    if (data) {
        if (data->simulation_active) {
            sem_destroy(&data->admin_sem);
            sem_destroy(&data->output_sem);
        }
        munmap(data, sizeof(SharedData));
    }
    
    if (shm_fd != -1) {
        close(shm_fd);
        shm_unlink(SHM_NAME);
    }
    
    std::cout << "Ресурсы освобождены" << std::endl;
}

void print_status() {
    sem_wait(&data->output_sem);
    std::cout << "\nСтатус гостиницы:" << std::endl;
    std::cout << "Одноместных номеров: " << data->single_rooms << "/7" << std::endl;
    std::cout << "Двухместных номеров: " << data->double_rooms << "/10" << std::endl;
    std::cout << "Леди в ожидании: " << data->waiting_ladies << std::endl;
    std::cout << "Джентльменов в ожидании: " << data->waiting_gentlemen << std::endl;
    std::cout << "Обслужено: " << data->served_clients << std::endl;
    std::cout << "Отказано: " << data->rejected_clients << std::endl;
    sem_post(&data->output_sem);
}

void client_process(Gender gender, int client_id) {
    std::string gender_str = (gender == LADY) ? "Леди" : "Джентльмен";
    std::string gender_came = (gender == LADY) ? "пришла" : "пришел";
    std::string gender_action = (gender == LADY) ? "заселилась" : "заселился";
    
    sleep(rand() % 2);
    
    sem_wait(&data->output_sem);
    std::cout << gender_str << " #" << client_id << " " << gender_came << " в гостиницу" << std::endl;
    sem_post(&data->output_sem);
    
    sleep(1);
    
    sem_wait(&data->admin_sem);
    
    bool got_room = false;
    std::string room_type = "";
    
    if (data->double_rooms > 0) {
        if (gender == LADY && data->waiting_ladies > 0) {
            data->waiting_ladies--;
            data->double_rooms--;
            got_room = true;
            room_type = "двухместный";
            data->served_clients += 2;
            
            sem_post(&data->admin_sem);
            
            sem_wait(&data->output_sem);
            std::cout << "Две леди заселились в двухместный номер!" << std::endl;
            sem_post(&data->output_sem);
            
            sleep(7 + (rand() % 4));
        }
        else if (gender == GENTLEMAN && data->waiting_gentlemen > 0) {
            data->waiting_gentlemen--;
            data->double_rooms--;
            got_room = true;
            room_type = "двухместный";
            data->served_clients += 2;
            
            sem_post(&data->admin_sem);
            
            sem_wait(&data->output_sem);
            std::cout << "Два джентльмена заселились в двухместный номер" << std::endl;
            sem_post(&data->output_sem);
            
            sleep(7 + (rand() % 4));
        }
    }
    
    if (!got_room) {
        if (data->single_rooms > 0) {
            data->single_rooms--;
            got_room = true;
            room_type = "одноместный";
            data->served_clients++;
            
            sem_post(&data->admin_sem);
            
            sem_wait(&data->output_sem);
            std::cout << gender_str << " #" << client_id << " " << gender_action << " в одноместный номер" << std::endl;
            sem_post(&data->output_sem);
            
            sleep(5 + (rand() % 4));
        }
        else if (data->double_rooms > 0) {
            if (gender == LADY) {
                data->waiting_ladies++;
            } else {
                data->waiting_gentlemen++;
            }
            
            sem_post(&data->admin_sem);
            
            sem_wait(&data->output_sem);
            std::cout << gender_str << " #" << client_id << " ждет соседа для двухместного номера" << std::endl;
            sem_post(&data->output_sem);
            
            bool paired = false;
            for (int i = 0; i < 10; i++) {
                sleep(1);
                
                sem_wait(&data->admin_sem);
                
                if (gender == LADY && data->waiting_ladies > 1 && data->double_rooms > 0) {
                    data->waiting_ladies -= 2;
                    data->double_rooms--;
                    paired = true;
                    room_type = "двухместный";
                    data->served_clients += 2;
                    sem_post(&data->admin_sem);
                    break;
                }
                else if (gender == GENTLEMAN && data->waiting_gentlemen > 1 && data->double_rooms > 0) {
                    data->waiting_gentlemen -= 2;
                    data->double_rooms--;
                    paired = true;
                    room_type = "двухместный";
                    data->served_clients += 2;
                    sem_post(&data->admin_sem);
                    break;
                }
                
                sem_post(&data->admin_sem);
                
                if (i % 3 == 0) {
                    sem_wait(&data->output_sem);
                    std::cout << gender_str << " #" << client_id << " ждет соседа" << std::endl;
                    sem_post(&data->output_sem);
                }
            }
            
            if (paired) {
                sem_wait(&data->output_sem);
                std::cout << gender_str << " #" << client_id << " " << gender_action << " в двухместный номер с соседом" << std::endl;
                sem_post(&data->output_sem);
                
                sleep(6 + (rand() % 3));
                got_room = true;
            } else {
                sem_wait(&data->admin_sem);
                if (gender == LADY && data->waiting_ladies > 0) {
                    data->waiting_ladies--;
                } else if (gender == GENTLEMAN && data->waiting_gentlemen > 0) {
                    data->waiting_gentlemen--;
                }
                data->rejected_clients++;
                sem_post(&data->admin_sem);
                
                sem_wait(&data->output_sem);
                std::cout << gender_str << " #" << client_id << " уходит - не дождался соседа" << std::endl;
                sem_post(&data->output_sem);
                _exit(0);
            }
        }
        else {
            data->rejected_clients++;
            sem_post(&data->admin_sem);
            
            sem_wait(&data->output_sem);
            std::cout << gender_str << " #" << client_id << " уходит - нет свободных номеров" << std::endl;
            sem_post(&data->output_sem);
            _exit(0);
        }
    }
    
    sleep(1);
    
    sem_wait(&data->admin_sem);
    if (room_type == "одноместный") {
        data->single_rooms++;
        if (data->single_rooms > 7) data->single_rooms = 7;
    } else if (room_type == "двухместный") {
        data->double_rooms++;
        if (data->double_rooms > 10) data->double_rooms = 10;
    }
    sem_post(&data->admin_sem);
    
    sem_wait(&data->output_sem);
    std::cout << gender_str << " #" << client_id << " освобождает " << room_type << " номер" << std::endl;
    sem_post(&data->output_sem);
    
    _exit(0);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (getpid() == getppid()) {
        atexit(cleanup);
    }
    
    int max_clients = 30;
    if (argc > 1) {
        max_clients = atoi(argv[1]);
        if (max_clients <= 0) max_clients = 30;
    }
    
    std::cout << "Программа на 4-6 баллов" << std::endl;
    std::cout << "Одноместных номеров: 7, двухместных: 10" << std::endl;
    std::cout << "Клиентов для симуляции: " << max_clients << std::endl;
    
    srand(time(nullptr));
    
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
    data->waiting_ladies = 0;
    data->waiting_gentlemen = 0;
    data->served_clients = 0;
    data->rejected_clients = 0;
    data->simulation_active = false;
    
    if (sem_init(&data->admin_sem, 1, 1) == -1) {
        perror("sem_init admin_sem");
        munmap(data, sizeof(SharedData));
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return 1;
    }
    
    if (sem_init(&data->output_sem, 1, 1) == -1) {
        perror("sem_init output_sem");
        sem_destroy(&data->admin_sem);
        munmap(data, sizeof(SharedData));
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return 1;
    }
    
    data->simulation_active = true;
    
    std::cout << "Инициализация завершена" << std::endl;
    
    for (int i = 0; i < max_clients && !stop; i++) {
        usleep(500000 + (rand() % 1000000));
        
        Gender gender = (rand() % 2 == 0) ? LADY : GENTLEMAN;
        
        pid_t pid = fork();
        if (pid == 0) {
            client_process(gender, i);
            _exit(0);
        } else if (pid < 0) {
            std::cerr << "Ошибка fork" << std::endl;
        }
        
        if ((i + 1) % 3 == 0) {
            print_status();
        }
    }
    
    std::cout << "\nОжидание завершения всех клиентов" << std::endl;
    
    while (wait(NULL) > 0);
    
    std::cout << "\nСимуляция завершена" << std::endl;
    print_status();
    
    std::cout << "\nИтоговая статистика:" << std::endl;
    std::cout << "Всего клиентов: " << max_clients << std::endl;
    std::cout << "Обслужено: " << data->served_clients << std::endl;
    std::cout << "Отказано: " << data->rejected_clients << std::endl;
    
    return 0;
}