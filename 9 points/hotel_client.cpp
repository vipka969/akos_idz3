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
#include <cstring>
#include <errno.h>
#include "shared_data.h"

volatile sig_atomic_t client_stop = 0;

void client_signal_handler(int signum) {
    (void)signum;
    client_stop = 1;
}

void send_to_observer(MessageType msg_type, Gender gender, int client_id, const std::string& details = "") {
    int fifo_fd = open(FIFO_NAME, O_WRONLY | O_NONBLOCK);
    if (fifo_fd == -1) {
        return;
    }
    
    Message msg = {};
    msg.type = msg_type;
    msg.gender = gender;
    msg.client_id = client_id;
    strncpy(msg.details, details.c_str(), sizeof(msg.details)-1);
    msg.details[sizeof(msg.details)-1] = '\0';
    
    ssize_t bytes = write(fifo_fd, &msg, sizeof(Message));
    if (bytes == -1 && errno != EAGAIN) {
    }
    
    close(fifo_fd);
}

void cleanup_clients(sem_t* sem_admin, sem_t* sem_output, SharedData* data, int shm_fd) {
    if (sem_admin) {
        sem_close(sem_admin);
    }
    if (sem_output) {
        sem_close(sem_output);
    }
    if (data) {
        munmap(data, sizeof(SharedData));
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

bool enter_hotel(int client_id, sem_t*& sem_admin, sem_t*& sem_output, SharedData*& data, int& shm_fd) {
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cout << "Клиент " << client_id << ": Гостиница не найдена! Сначала запустите ./hotel_admin" << std::endl;
        return false;
    }
    
    data = static_cast<SharedData*>(
        mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)
    );
    
    if (data == MAP_FAILED) {
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
    SharedData* data = nullptr;
    int shm_fd = -1;
    
    if (!enter_hotel(client_id, sem_admin, sem_output, data, shm_fd)) {
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
    
    send_to_observer(MSG_CLIENT_ARRIVED, gender, client_id, "прибыл в гостиницу");
    
    sleep(1);
    
    sem_wait(sem_admin);
    
    if (!data->hotel_active || client_stop) {
        sem_post(sem_admin);
        sem_wait(sem_output);
        std::cout << "Клиент " << client_id << ": Гостиница закрыта" << std::endl;
        sem_post(sem_output);
        send_to_observer(MSG_CLIENT_REJECTED, gender, client_id, "гостиница закрыта");
        cleanup_clients(sem_admin, sem_output, data, shm_fd);
        return;
    }
    
    bool got_room = false;
    std::string room_type = "";
    pid_t my_pid = getpid();
    
    if (data->single_rooms > 0) {
        data->single_rooms--;
        got_room = true;
        room_type = "одноместный";
        data->served_clients++;
        
        sem_post(sem_admin);
        
        sem_wait(sem_output);
        std::cout << "Клиент " << client_id << " (" << gender_str << "): " << gender_action << " в одноместный номер" << std::endl;
        sem_post(sem_output);
        
        send_to_observer(MSG_CLIENT_SERVED, gender, client_id, "одноместный номер");
        
        sleep(6 + (rand() % 7));
    }
    else if (data->double_rooms > 0) {
        bool found_waiting = false;
        if (gender == LADY) {
            found_waiting = mark_client(data->waiting_ladies);
        } else {
            found_waiting = mark_client(data->waiting_gentlemen);
        }
        
        if (found_waiting) {
            data->double_rooms--;
            got_room = true;
            room_type = "двухместный";
            data->served_clients += 2;
            
            sem_post(sem_admin);
            
            sem_wait(sem_output);
            if (gender == LADY) {
                std::cout << "Клиент " << client_id << ": Две леди заселились в двухместный номер" << std::endl;
            } else {
                std::cout << "Клиент " << client_id << ": Два джентльмена заселились в двухместный номер" << std::endl;
            }
            sem_post(sem_output);
            
            send_to_observer(MSG_CLIENT_SERVED, gender, client_id, "двухместный номер с соседом");
            
            sleep(8 + (rand() % 8));
        }
        else {
            bool added = false;
            if (gender == LADY) {
                added = add_to_queue(my_pid, data->waiting_ladies);
            } else {
                added = add_to_queue(my_pid, data->waiting_gentlemen);
            }
            
            if (!added) {
                data->rejected_clients++;
                sem_post(sem_admin);
                
                sem_wait(sem_output);
                std::cout << "Клиент " << client_id << " (" << gender_str << "): очередь переполнена, ухожу" << std::endl;
                sem_post(sem_output);
                
                send_to_observer(MSG_CLIENT_REJECTED, gender, client_id, "очередь переполнена");
                cleanup_clients(sem_admin, sem_output, data, shm_fd);
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
                
                bool still_waiting = in_queue(my_pid, 
                    (gender == LADY) ? data->waiting_ladies : data->waiting_gentlemen);
                
                if (!still_waiting && data->double_rooms > 0) {
                    found_pair = true;
                    room_type = "двухместный";
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
                
                send_to_observer(MSG_CLIENT_SERVED, gender, client_id, "двухместный номер (дождался соседа)");
                
                sleep(7);
                got_room = true;
            } else if (!client_stop) {
                sem_wait(sem_admin);
                if (gender == LADY) {
                    remove_client(my_pid, data->waiting_ladies);
                } else {
                    remove_client(my_pid, data->waiting_gentlemen);
                }
                data->rejected_clients++;
                sem_post(sem_admin);
                
                sem_wait(sem_output);
                std::cout << "Клиент " << client_id << " (" << gender_str << "): уходит - не дождался соседа" << std::endl;
                sem_post(sem_output);
                
                send_to_observer(MSG_CLIENT_REJECTED, gender, client_id, "не дождался соседа");
                cleanup_clients(sem_admin, sem_output, data, shm_fd);
                return;
            }
        }
    }
    else if (!client_stop) {
        data->rejected_clients++;
        sem_post(sem_admin);
        
        sem_wait(sem_output);
        std::cout << "Клиент " << client_id << " (" << gender_str << "): нет свободных номеров, ухожу" << std::endl;
        sem_post(sem_output);
        
        send_to_observer(MSG_CLIENT_REJECTED, gender, client_id, "нет свободных номеров");
        cleanup_clients(sem_admin, sem_output, data, shm_fd);
        return;
    }
    
    if (got_room && !client_stop) {
        sem_wait(sem_admin);
        if (room_type == "одноместный") {
            data->single_rooms++;
            if (data->single_rooms > 7) {
                data->single_rooms = 7;
            }
        } else if (room_type == "двухместный") {
            data->double_rooms++;
            if (data->double_rooms > 10) {
                data->double_rooms = 10;
            }
        }
        sem_post(sem_admin);
        
        sem_wait(sem_output);
        std::cout << "Клиент " << client_id << " (" << gender_str << "): освобождает " << room_type << " номер" << std::endl;
        sem_post(sem_output);
        
        send_to_observer(MSG_CLIENT_DEPARTED, gender, client_id, "освобождает " + room_type + " номер");
    }
    
    cleanup_clients(sem_admin, sem_output, data, shm_fd);
}

int main(int argc, char* argv[]) {
    srand(time(nullptr) + getpid());
    
    if (argc > 1) {
        int num_clients = atoi(argv[1]);
        if (num_clients < 1) {
            num_clients = 1;
        }
        
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