#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <string>

const char* SHM_NAME = "/hotel_shm_9_points";
const char* SEM_ADMIN_NAME = "/hotel_sem_admin_9_points";
const char* SEM_OUTPUT_NAME = "/hotel_sem_output_9_points";
const char* FIFO_NAME = "/tmp/hotel_fifo_9_points";

const int MAX_WAITING_CLIENTS = 50;

enum Gender { 
    LADY, 
    GENTLEMAN 
};

enum MessageType {
    MSG_CLIENT_ARRIVED,
    MSG_CLIENT_SERVED,
    MSG_CLIENT_REJECTED,
    MSG_CLIENT_DEPARTED,
    MSG_STATUS_UPDATE
};

struct WaitingClient {
    pid_t pid;
    bool is_matched;
};

struct Message {
    MessageType type;
    Gender gender;
    int client_id;
    int single_rooms;
    int double_rooms;
    int waiting_ladies;
    int waiting_gentlemen;
    int total_served;
    int total_rejected;
    char details[100];
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