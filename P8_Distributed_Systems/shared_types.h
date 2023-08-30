#ifndef SHARED_TYPES_H
#define SHARED_TYPES_H

typedef struct {
    int opcode;         // 0=idle, 1=get, 2=put
    int client_id;
    int seq_number;
    int time;           // for idle
    int key;
    int value;
} client_request_t;

typedef struct {
    int client_id;
    int seq_number;
    int ack;            // 1 for in progress, 0 for done
    int return_val;
} server_feedback_t;

#endif
