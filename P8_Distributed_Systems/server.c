#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>

#include "server_functions.h"
#include "udp.h"
#include "shared_types.h"

struct table_entry {
    int client_id;
    int seq_num;
    int done;
    int result;
};

struct table_entry table[100];
//pthread_mutex_t tablelock = PTHREAD_MUTEX_INITIALIZER;
struct socket my_socket;

void *serve(void* arg) {
    struct packet_info *client_packet = (struct packet_info*) arg;
    client_request_t *req = (client_request_t*)client_packet->buf; // parse packet info
    server_feedback_t *fdbk = (server_feedback_t*)malloc(sizeof(server_feedback_t));
    fdbk->client_id = req->client_id;
    fdbk->seq_number = req->seq_number;

    // search table
    int found_idx = -1;
    int next_idx = -1;
    //pthread_mutex_lock(&tablelock);
    for (int i = 0; i < 100; i++) {
        if (table[i].client_id == -1) {
            next_idx = i;
            break;
        }
        if (table[i].client_id == req->client_id) {
            found_idx = i;
            break;
        }
    }
    //printf("found: %d, next: %d\n", found_idx, next_idx);
    if(next_idx != -1) { // if id not found, assign a new entry
        table[next_idx].client_id = req->client_id;
        found_idx = next_idx;
    }
    int temp_seqnum = table[found_idx].seq_num;
    //pthread_mutex_unlock(&tablelock);
    
    if (temp_seqnum > req->seq_number) { // old request
//        printf("ignored client id %d, seq num %d\n", req->client_id, req->seq_number);
        // ignore
    }
    else if (temp_seqnum == req->seq_number) { // same request
        //pthread_mutex_lock(&tablelock);
        if (table[found_idx].done) { // request done, resend packet
            fdbk->ack = 0; // done, no ack, send result
            fdbk->return_val = table[found_idx].result;
//            printf("resent client id %d, seq num %d\n", req->client_id, req->seq_number);
        }
        else { // not done, ack
            fdbk->ack = 1;
            fdbk->return_val = -1; // dont care
//            printf("acked client id %d, seq num %d\n", req->client_id, req->seq_number);
        }
        //pthread_mutex_unlock(&tablelock);
        send_packet(my_socket, client_packet->sock, client_packet->slen, (char*)fdbk, sizeof(server_feedback_t));
    }
    else { // new request
        //pthread_mutex_lock(&tablelock);
        table[found_idx].done = 0; // clear done
        table[found_idx].seq_num = req->seq_number; // increment seq num
        //pthread_mutex_unlock(&tablelock);
        if (req->opcode == 0) {
            idle(req->time);
            //pthread_mutex_lock(&tablelock);
            table[found_idx].done = 1;
            //pthread_mutex_unlock(&tablelock);
        }
        else if (req->opcode == 1) {
            int func_result = get(req->key);
//            printf("client id %d, seq num %d, got %d\n", req->client_id, req->seq_number, func_result);
            //pthread_mutex_lock(&tablelock);
            table[found_idx].done = 1;
            table[found_idx].result = func_result;
            //pthread_mutex_unlock(&tablelock);
        }
        else {
            int func_result = put(req->key,req->value);
//            printf("client id %d, seq num %d, put %d at %d\n", req->client_id, req->seq_number, req->value, req->key);
            //pthread_mutex_lock(&tablelock);
            table[found_idx].done = 1;
            table[found_idx].result = func_result;
            //pthread_mutex_unlock(&tablelock);
        }
    }
    free(fdbk);
    pthread_exit(NULL);
    //return NULL; // a matching client id was found anyway
}

int main(int argc, char *argv[])
{
    int port = atoi(argv[1]);
    for (int i = 0; i < 100; i++) {     // table init
        table[i].client_id = -1;
        table[i].seq_num = -1;
        table[i].done = 0;
        table[i].result = -1;
    }
    my_socket = init_socket(port);
    
    while (1) {
        struct packet_info recent_packet = receive_packet(my_socket);
        pthread_t thread1;
        pthread_create(&thread1, NULL, serve, &recent_packet);
        client_request_t *req = (client_request_t*)recent_packet.buf; // parse packet info
        if (req->opcode != 0) {
            pthread_join(thread1, NULL);
        }
    }

    return 0;
}
