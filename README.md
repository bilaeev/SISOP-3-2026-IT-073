## Soal 1
## Penjelasan kode soal 1
Ketiga file membentuk satu sistem client-server dengan protokol komunikasi yang sama.
> protocol.h berfungsi sebagai definisi aturan komunikasi (protokol), berisi konstanta tipe pesan, RPC, dan struktur data Packet yang digunakan oleh server dan client.

> Wired.c adalah server yang menerima koneksi dari banyak client, mengelola data client, melakukan broadcast pesan, menjalankan RPC untuk admin, serta mencatat log.

> Navi.c adalah client yang terhubung ke server, mengirim pesan, menerima pesan secara asynchronous, serta menyediakan interface admin jika login sebagai “The Knights”.

> Semua komunikasi antara client dan server menggunakan socket dan dikirim dalam bentuk struct Packet. Ini merupakan implementasi message passing pada IPC (Modul 3).  
## Penjelasan protocol.h
File ini berisi definisi protokol komunikasi.  
Bagian alamat server:
```c
#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 9000
```
Digunakan client untuk melakukan koneksi ke server.  
Tipe pesan client ke server:
```c
#define MSG_REGISTER    1
#define MSG_CHAT        2
#define MSG_EXIT        3
#define MSG_ADMIN_AUTH  4
#define MSG_RPC         5
```
Tipe pesan server ke client:
```c
#define MSG_ACCEPT      10
#define MSG_NAME_TAKEN  11
#define MSG_BROADCAST   12
#define MSG_RPC_RESP    13
#define MSG_SHUTDOWN    14
````
RPC command:
```c
#define RPC_GET_USERS   1
#define RPC_GET_UPTIME  2
#define RPC_SHUTDOWN    3
```
Struktur data utama:
```c
typedef struct {
    int  type;
    char sender[MAX_NAME];
    char body[MAX_MSG];
} Packet;
```
Struct ini digunakan dalam semua komunikasi. Ini adalah implementasi message passing (Modul 3).
## Penjelasan Wired.c (Server)
File ini adalah server utama yang menangani banyak client secara bersamaan.  
Struktur data client:
```c
typedef struct {
    int     fd;
    char    name[MAX_NAME];
    int     is_admin;
    int     active;
} Client;
```
Digunakan untuk menyimpan informasi tiap client.
```c
pthread_mutex_t clients_mtx = PTHREAD_MUTEX_INITIALIZER;
```
Digunakan untuk menghindari race condition saat banyak thread mengakses data client.  
Contoh penggunaan:
```c
pthread_mutex_lock(&clients_mtx);
...
pthread_mutex_unlock(&clients_mtx);
Broadcast (Message Passing – Modul 3)
```
```c
static void broadcast(Packet *pkt, int exclude_fd) {
    pthread_mutex_lock(&clients_mtx);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && !clients[i].is_admin &&
            clients[i].fd != exclude_fd) {
            send_packet(clients[i].fd, pkt);
        }
    }
    pthread_mutex_unlock(&clients_mtx);
}
```
Server mengirim pesan ke semua client aktif kecuali pengirim. -Multithreading (Modul 3)
```c
pthread_create(&tid, NULL, client_thread, targ);
pthread_detach(tid);
```
Setiap client ditangani oleh thread sendiri sehingga server tidak terblokir oleh satu client. Fungsi client_thread, fungsi ini menangani seluruh komunikasi dengan satu client.  
Registrasi nama:
```c
if (pkt.type != MSG_REGISTER) continue;

int taken = name_exists(pkt.sender);
```
Jika nama sudah digunakan, server mengirim MSG_NAME_TAKEN. Jika tidak, nama diterima.  
Autentikasi admin:
```c
if (strcmp(c->name, ADMIN_NAME) == 0)
```
```c
if (pkt.type == MSG_ADMIN_AUTH &&
    strcmp(pkt.body, ADMIN_PASSWORD) == 0)
```
Hanya admin yang bisa mengakses RPC.  
Chat:
```c
if (pkt.type == MSG_CHAT && !c->is_admin)
```
```c
broadcast(&bcast, fd);
```
Pesan dari client dikirim ke semua client lain.  
Exit:
```c
if (pkt.type == MSG_EXIT)
```
Client keluar secara normal. -RPC (Modul 3)
```c
if (pkt.type == MSG_RPC && c->is_admin)
```
```c
handle_rpc(idx, cmd);
```
Menjalankan perintah admin seperti: jumlah user aktif, uptime server, shutdown server.  
Contoh RPC:  
```c
case RPC_GET_USERS:
    int n = count_users();
```
```c
case RPC_GET_UPTIME:
    long uptime = (long)(time(NULL) - server_start);
```
```c
case RPC_SHUTDOWN:
    exit(0);
Logging (Modul 2 – File Handling)
```
```c
log_fp = fopen("history.log", "a");
```
```c
fprintf(log_fp, "[%s] [%s] [%s]\n", ts, category, message);
Setiap aktivitas disimpan ke file.
Socket Programming (Modul 3)
```
```c
server_fd = socket(AF_INET, SOCK_STREAM, 0);
bind(...)
listen(...)
accept(...)
```
Server menerima koneksi dari client.
## Penjelasan Navi.c (Client)
Client digunakan untuk berkomunikasi dengan server.  -Socket Connection (Modul 3)
```c
connect(sock_fd, (struct sockaddr *)&srv, sizeof(srv))
```
Client terhubung ke server. -Thread Receiver (Asynchronous – Modul 3)
```c
pthread_create(&rtid, NULL, recv_thread, NULL);
```
Fungsi:
```c
int n = recv(sock_fd, &pkt, sizeof(pkt), 0);
```
Thread ini terus menerima pesan dari server tanpa mengganggu input user.  
Registrasi nama
```c
pkt.type = MSG_REGISTER;
send(sock_fd, &pkt, sizeof(pkt), 0);
```
Client mengirim nama ke server dan menunggu respon.  
Admin login
```c
if (strcmp(resp.body, "NEED_PASSWORD") == 0)
```
```c
pkt.type = MSG_ADMIN_AUTH;
```
Jika nama adalah “The Knights”, client diminta password.  
Chat
```c
pkt.type = MSG_CHAT;
send(sock_fd, &pkt, sizeof(pkt), 0);
```
Mengirim pesan ke server.  
Exit
```c
pkt.type = MSG_EXIT;
Client keluar dari server.
Admin Console (RPC)
```
```c
pkt.type = MSG_RPC;
snprintf(pkt.body, MAX_MSG, "%d", cmd);
```
Mengirim perintah RPC ke server, mapping ke Modul
