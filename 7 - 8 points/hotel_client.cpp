#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <csignal>
#include <semaphore.h>
#include <vector>
#include <sys/wait.h>
#include "shared_data.h"

volatile sig_atomic_t client_stop = 0;

void client_signal_handler(int signum) {
    (void)signum;
    client_stop = 1;
}

void cleanup_clients(sem_t* sem_admin, sem_t* sem_output, SharedData* shared_data, int shm_fd) {
    if (sem_admin) {
        sem_close(sem_admin);
    }
    if (sem_output) {
        sem_close(sem_output);
    }
    if (shared_data) {
        munmap(shared_data, sizeof(SharedData));
    }
    if (shm_fd != -1) {
        close(shm_fd);
    }
}

bool in_queue(pid_t client_pid, const WaitingClient queue[]) {
    for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
        if (queue[i].pid == client_pid && !queue[i].is_matched) {
            return true;
        }
    }
    return false;
}

bool add_to_queue(pid_t client_pid, WaitingClient queue[]) {
    for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
        if (queue[i].pid == 0) {
            queue[i].pid = client_pid;
            queue[i].is_matched = false;
            return true;
        }
    }
    return false;
}

bool mark_client(WaitingClient queue[]) {
    for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
        if (queue[i].pid != 0 && !queue[i].is_matched) {
            queue[i].is_matched = true;
            return true;
        }
    }
    return false;
}

void remove_client(pid_t client_pid, WaitingClient queue[]) {
    for (int i = 0; i < MAX_WAITING_CLIENTS; i++) {
        if (queue[i].pid == client_pid) {
            queue[i].pid = 0;
            queue[i].is_matched = false;
            break;
        }
    }
}

bool enter_hotel(int client_id, sem_t*& sem_admin, sem_t*& sem_output, SharedData*& shared_data, int& shm_fd) {
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cout << "Клиент " << client_id << ": Гостиница не найдена! Сначала запустите ./hotel_admin" << std::endl;
        return false;
    }
    
    shared_data = static_cast<SharedData*>(
        mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)
    );
    
    if (shared_data == MAP_FAILED) {
        std::cout << "Клиент " << client_id << ": Ошибка доступа к гостинице" << std::endl;
        return false;
    }
    
    sem_admin = sem_open(SEM_ADMIN_NAME, 0);
    if (sem_admin == SEM_FAILED) {
        std::cout << "Клиент " << client_id << ": Гостиница закрыта" << std::endl;
        return false;
    }
    
    sem_output = sem_open(SEM_OUTPUT_NAME, 0);
    if (sem_output == SEM_FAILED) {
        std::cout << "Клиент " << client_id << ": Гостиница закрыта" << std::endl;
        return false;
    }
    
    return true;
}

void client_logic(int client_id) {
    signal(SIGINT, client_signal_handler);
    signal(SIGTERM, client_signal_handler);
    
    sem_t* sem_admin = nullptr;
    sem_t* sem_output = nullptr;
    SharedData* shared_data = nullptr;
    int shm_fd = -1;
    
    if (!enter_hotel(client_id, sem_admin, sem_output, shared_data, shm_fd)) {
        return;
    }
    
    Gender gender = (rand() % 2 == 0) ? LADY : GENTLEMAN;
    std::string gender_str = (gender == LADY) ? "Леди" : "Джентльмен";
    std::string gender_came = (gender == LADY) ? "пришла" : "пришел";
    std::string gender_action = (gender == LADY) ? "заселилась" : "заселился";
    
    sleep(1 + (rand() % 3));
    
    sem_wait(sem_output);
    std::cout << "Клиент " << client_id << " (" << gender_str << "): " << gender_came << " в гостиницу" << std::endl;
    sem_post(sem_output);
    
    sleep(1);
    
    sem_wait(sem_admin);
    
    if (!shared_data->hotel_active || client_stop) {
        sem_post(sem_admin);
        sem_wait(sem_output);
        std::cout << "Клиент " << client_id << ": Гостиница закрыта" << std::endl;
        sem_post(sem_output);
        cleanup_clients(sem_admin, sem_output, shared_data, shm_fd);
        return;
    }
    
    bool got_room = false;
    std::string room_type = "";
    pid_t my_pid = getpid();
    
    if (shared_data->double_rooms > 0) {
        if (gender == LADY && mark_client(shared_data->waiting_ladies)) {
            shared_data->double_rooms--;
            got_room = true;
            room_type = "двухместный";
            shared_data->served_clients += 1;
            
            sem_post(sem_admin);
            
            sem_wait(sem_output);
            std::cout << "Клиент " << client_id << ": Две леди заселились в двухместный номер" << std::endl;
            sem_post(sem_output);
            
            for (int i = 0; i < 8 + (rand() % 3) && !client_stop; i++) {
                sleep(1);
            }
        }
        else if (gender == GENTLEMAN && mark_client(shared_data->waiting_gentlemen)) {
            shared_data->double_rooms--;
            got_room = true;
            room_type = "двухместный";
            shared_data->served_clients += 1;
            
            sem_post(sem_admin);
            
            sem_wait(sem_output);
            std::cout << "Клиент " << client_id << ": Два джентльмена заселились в двухместный номер" << std::endl;
            sem_post(sem_output);
            
            for (int i = 0; i < 8 + (rand() % 3) && !client_stop; i++) {
                sleep(1);
            }
        }
    }
    
    if (!got_room && !client_stop) {
        if (shared_data->single_rooms > 0) {
            shared_data->single_rooms--;
            got_room = true;
            room_type = "одноместный";
            shared_data->served_clients++;
            
            sem_post(sem_admin);
            
            sem_wait(sem_output);
            std::cout << "Клиент " << client_id << " (" << gender_str << "): " << gender_action << " в одноместный номер" << std::endl;
            sem_post(sem_output);
            
            for (int i = 0; i < 6 + (rand() % 3) && !client_stop; i++) {
                sleep(1);
            }
        }
        else if (shared_data->double_rooms > 0 && !client_stop) {
            bool added_to_queue = false;
            if (gender == LADY) {
                added_to_queue = add_to_queue(my_pid, shared_data->waiting_ladies);
            } else {
                added_to_queue = add_to_queue(my_pid, shared_data->waiting_gentlemen);
            }
            
            if (!added_to_queue) {
                shared_data->rejected_clients++;
                sem_post(sem_admin);
                
                sem_wait(sem_output);
                std::cout << "Клиент " << client_id << " (" << gender_str << "): очередь переполнена, ухожу" << std::endl;
                sem_post(sem_output);
                
                cleanup_clients(sem_admin, sem_output, shared_data, shm_fd);
                return;
            }
            
            sem_post(sem_admin);
            
            sem_wait(sem_output);
            std::cout << "Клиент " << client_id << " (" << gender_str << "): ждет соседа для двухместного номера" << std::endl;
            sem_post(sem_output);
            
            bool found_pair = false;
            for (int i = 0; i < 10 && !found_pair && !client_stop; i++) {
                sleep(1);
                
                sem_wait(sem_admin);
                
                bool still_waiting = false;
                if (gender == LADY) {
                    still_waiting = in_queue(my_pid, shared_data->waiting_ladies);
                } else {
                    still_waiting = in_queue(my_pid, shared_data->waiting_gentlemen);
                }
                
                if (!still_waiting && shared_data->double_rooms > 0) {
                    shared_data->double_rooms--;
                    found_pair = true;
                    room_type = "двухместный";
                    shared_data->served_clients += 1;
                    sem_post(sem_admin);
                    break;
                }
                sem_post(sem_admin);
                
                if (i % 2 == 0 && !client_stop) {
                    sem_wait(sem_output);
                    std::cout << "Клиент " << client_id << " (" << gender_str << "): ждет" << std::endl;
                    sem_post(sem_output);
                }
            }
            
            if (found_pair && !client_stop) {
                sem_wait(sem_output);
                std::cout << "Клиент " << client_id << " (" << gender_str << "): " << gender_action << " в двухместный номер с соседом" << std::endl;
                sem_post(sem_output);
                
                for (int i = 0; i < 7 && !client_stop; i++) {
                    sleep(1);
                }
                got_room = true;
            } else if (!client_stop) {
                sem_wait(sem_admin);
                if (gender == LADY) {
                    remove_client(my_pid, shared_data->waiting_ladies);
                } else {
                    remove_client(my_pid, shared_data->waiting_gentlemen);
                }
                shared_data->rejected_clients++;
                sem_post(sem_admin);
                
                sem_wait(sem_output);
                std::cout << "Клиент " << client_id << " (" << gender_str << "): уходит - не дождался соседа" << std::endl;
                sem_post(sem_output);
                
                cleanup_clients(sem_admin, sem_output, shared_data, shm_fd);
                return;
            }
        }
        else if (!client_stop) {
            shared_data->rejected_clients++;
            sem_post(sem_admin);
            
            sem_wait(sem_output);
            std::cout << "Клиент " << client_id << " (" << gender_str << "): нет свободных номеров, ухожу" << std::endl;
            sem_post(sem_output);
            
            cleanup_clients(sem_admin, sem_output, shared_data, shm_fd);
            return;
        }
    }
    
    if (got_room && !client_stop) {
        sem_wait(sem_admin);
        if (room_type == "одноместный") {
            shared_data->single_rooms++;
            if (shared_data->single_rooms > 7) {
                shared_data->single_rooms = 7;
            }
        } else {
            shared_data->double_rooms++;
            if (shared_data->double_rooms > 10) {
                shared_data->double_rooms = 10;
            }
        }
        sem_post(sem_admin);
        
        sem_wait(sem_output);
        std::cout << "Клиент " << client_id << " (" << gender_str << "): освобождает " << room_type << " номер" << std::endl;
        sem_post(sem_output);
    }
    
    cleanup_clients(sem_admin, sem_output, shared_data, shm_fd);
}

int main(int argc, char* argv[]) {
    srand(time(nullptr) + getpid());
    
    if (argc > 1) {
        int num_clients = atoi(argv[1]);
        if (num_clients < 1) num_clients = 1;
        
        std::cout << "Запуск " << num_clients << " клиентов" << std::endl;
        
        std::vector<pid_t> child_pids;
        
        for (int i = 0; i < num_clients; i++) {
            pid_t pid = fork();
            
            if (pid == 0) {
                client_logic(1000 + i);
                exit(0);
            } else if (pid > 0) {
                child_pids.push_back(pid);
            } else {
                std::cout << "Ошибка создания клиента " << i << std::endl;
            }
            
            usleep(500000 + (rand() % 1000000));
        }
        
        for (size_t i = 0; i < child_pids.size(); i++) {
            waitpid(child_pids[i], NULL, 0);
        }
        
        std::cout << "Все клиенты завершили работу" << std::endl;
    }
    else {
        client_logic(getpid());
    }
    
    return 0;
}