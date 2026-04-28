#ifndef PROTOCOL_H
#define PROTOCOL_H

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 9000

#define MAX_NAME    64
#define MAX_MSG     512

// Message types (client -> server)
#define MSG_REGISTER    1   // kirim nama
#define MSG_CHAT        2   // pesan biasa
#define MSG_EXIT        3   // /exit
#define MSG_ADMIN_AUTH  4   // autentikasi admin
#define MSG_RPC         5   // perintah admin RPC

// Message types (server -> client)
#define MSG_ACCEPT      10  // nama diterima
#define MSG_NAME_TAKEN  11  // nama sudah ada
#define MSG_BROADCAST   12  // pesan broadcast
#define MSG_RPC_RESP    13  // balasan RPC
#define MSG_SHUTDOWN    14  // server shutdown

// RPC Commands
#define RPC_GET_USERS   1
#define RPC_GET_UPTIME  2
#define RPC_SHUTDOWN    3

#define ADMIN_NAME      "The Knights"
#define ADMIN_PASSWORD  "protocol7"

typedef struct {
    int  type;
    char sender[MAX_NAME];
    char body[MAX_MSG];
} Packet;

#endif
