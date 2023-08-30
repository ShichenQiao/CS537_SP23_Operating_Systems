#ifndef CLIENT_H
#define CLIENT_H

#include "udp.h"

#define RETRY_COUNT 5
#define TIMEOUT_TIME 1

struct rpc_connection{
    struct socket recv_socket;
    struct sockaddr dst_addr;
    socklen_t dst_len;
    int seq_number;
    int client_id;
};

// initializes the RPC connection to the server
struct rpc_connection RPC_init(int src_port, int dst_port, char dst_addr[]);

// Sleeps the server thread for a few seconds
void RPC_idle(struct rpc_connection *rpc, int time);

// gets the value of a key on the server store
int RPC_get(struct rpc_connection *rpc, int key);

// sets the value of a key on the server store
int RPC_put(struct rpc_connection *rpc, int key, int value);

// closes the RPC connection to the server
void RPC_close(struct rpc_connection *rpc);

#endif
