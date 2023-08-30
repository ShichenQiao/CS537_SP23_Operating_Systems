#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "client.h"
#include "udp.h"
#include "shared_types.h"

// initializes the RPC connection to the server
struct rpc_connection RPC_init(int src_port, int dst_port, char dst_addr[])
{
    struct rpc_connection rpc;

    struct socket src = init_socket(src_port);
    rpc.recv_socket = src;

    struct sockaddr_storage addr;
    socklen_t addrlen;
    populate_sockaddr(AF_INET, dst_port, dst_addr, &addr, &addrlen);
    rpc.dst_addr = *((struct sockaddr *)(&addr));
    rpc.dst_len = addrlen;

    rpc.seq_number = 0;

    srand(time(NULL) ^ (getpid() << 16));
    rpc.client_id = rand();

    return rpc;
}

int send_req_with_retry(struct rpc_connection *rpc, client_request_t *req)
{
    int return_val = -1;

    int retry = 0;
    while(1) {
        // Retrying RPC requests on a chosen (short) timeout interval up to 5 times
        if(retry++ == 5) {
//            printf("client id %d, seq num %d, opcode %d, retry %d\n", req->client_id, req->seq_number, req->opcode, retry);
            die("server died");
        }
        // Sending idle, get, or put requests to the RPC server.
        send_packet(rpc->recv_socket, rpc->dst_addr, rpc->dst_len, (char*) req, sizeof(client_request_t));
//        printf("sent: client-%d seq-%d\n", rpc->client_id, rpc->seq_number);
        // Blocking with a 1s timeout until response from the server
        struct packet_info pkg = receive_packet_timeout(rpc->recv_socket, 1);
        if (pkg.recv_len != -1) {
            server_feedback_t fb = *(server_feedback_t *) pkg.buf;
//            printf("received: client-%d seq-%d ack: %d\n",fb.client_id, fb.seq_number, fb.ack);
            // Ignore packets that are for other client ids, or to old sequence numbers
            if (rpc->client_id == fb.client_id && rpc->seq_number == fb.seq_number) {
                if (fb.ack) {
                    // correct ACK received, reset retry count
                    retry = 0;
                    // Delaying retrys for 1 second on receiving an ACK from the server
                    sleep(1);
                }
                else {
                    return_val = fb.return_val;
                    rpc->seq_number++;
                    break;
                }
            }
        }
    }

    return return_val;
}

// Sleeps the server thread for a few seconds
void RPC_idle(struct rpc_connection *rpc, int time)
{
    client_request_t *req = malloc(sizeof(client_request_t));
    req->opcode = 0;
    req->client_id = rpc->client_id;
    req->seq_number = rpc->seq_number;
    req->time = time;
    req->key = -1;
    req->value = -1;

    send_req_with_retry(rpc, req);

    free(req);
}

// gets the value of a key on the server store
int RPC_get(struct rpc_connection *rpc, int key)
{
    client_request_t *req = malloc(sizeof(client_request_t));
    req->opcode = 1;
    req->client_id = rpc->client_id;
    req->seq_number = rpc->seq_number;
    req->time = -1;
    req->key = key;
    req->value = -1;

    int return_val = send_req_with_retry(rpc, req);

    free(req);

    return return_val;
}

// sets the value of a key on the server store
int RPC_put(struct rpc_connection *rpc, int key, int value)
{
    client_request_t *req = malloc(sizeof(client_request_t));
    req->opcode = 2;
    req->client_id = rpc->client_id;
    req->seq_number = rpc->seq_number;
    req->time = -1;
    req->key = key;
    req->value = value;

    int return_val = send_req_with_retry(rpc, req);

    free(req);

    return return_val;
}

// closes the RPC connection to the server
void RPC_close(struct rpc_connection *rpc)
{
    close_socket(rpc->recv_socket);
}