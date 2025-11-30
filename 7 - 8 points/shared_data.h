#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <semaphore.h>

const char* SHM_NAME = "/hotel_shm_78";
const char* SEM_ADMIN_NAME = "/hotel_sem_admin_78";
const char* SEM_OUTPUT_NAME = "/hotel_sem_output_78";

const int MAX_WAITING_CLIENTS = 20;

enum Gender { 
    LADY, 
    GENTLEMAN 
};

struct WaitingClient {
    pid_t pid;
    bool is_matched;
};

struct SharedData {
    int single_rooms;
    int double_rooms;
    int served_clients;
    int rejected_clients;
    bool hotel_active;
    
    WaitingClient waiting_ladies[MAX_WAITING_CLIENTS];
    WaitingClient waiting_gentlemen[MAX_WAITING_CLIENTS];
};

#endif