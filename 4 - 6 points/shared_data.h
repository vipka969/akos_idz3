#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <semaphore.h>

const char* SHM_NAME = "/hotel_shm_46";

enum Gender { 
    LADY, 
    GENTLEMAN 
};

struct SharedData {
    int single_rooms;
    int double_rooms;  
    int waiting_ladies;
    int waiting_gentlemen;
    int served_clients;
    int rejected_clients;
    bool simulation_active;
    
    sem_t admin_sem;
    sem_t output_sem;
};

#endif