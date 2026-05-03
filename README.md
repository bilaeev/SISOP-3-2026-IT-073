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

## Output
1. menjalankan server  
   <img width="467" height="40" alt="Screenshot 2026-05-03 190855" src="https://github.com/user-attachments/assets/21b34da7-561b-44b3-bfb0-ae8adc3044a4" />  
2. connect ke tiap client  
   <img width="492" height="76" alt="Screenshot 2026-05-03 190956" src="https://github.com/user-attachments/assets/36dad913-d10f-4bde-bfb8-db4c93478e2d" />  
3. error handling untuk pemanggilan navi apabila server mati  
   <img width="444" height="57" alt="Screenshot 2026-05-03 191113" src="https://github.com/user-attachments/assets/ec3d1bb2-fddd-453d-8470-c0f57319e9c1" />  
4. error handling apabila server sudah online tapi ada pemanggilan wired lagi di terminal lain  
   <img width="500" height="75" alt="Screenshot 2026-05-03 191327" src="https://github.com/user-attachments/assets/314f0e3f-8b8e-458d-9bb3-72aa786ea8c2" />  
5. mendapatkan pesan dari client lain yang aktif  
   TERMINAL 1  
   <img width="498" height="155" alt="Screenshot 2026-05-03 192709" src="https://github.com/user-attachments/assets/48071b60-d6e0-4d27-a0d5-2f1aae9a0caf" />  
   TERMINAL 2  
   <img width="500" height="75" alt="Screenshot 2026-05-03 191327" src="https://github.com/user-attachments/assets/49276999-8af3-4a36-92ba-d90935cbfbab" />  
6. error handling apabila server mati  
   TERMINAL CLIENT  
   <img width="511" height="96" alt="Screenshot 2026-05-03 193021" src="https://github.com/user-attachments/assets/e6816be7-f327-4389-8548-f0bbcb6207bf" />  
   TERMINAL SERVER  
   <img width="456" height="82" alt="Screenshot 2026-05-03 193101" src="https://github.com/user-attachments/assets/df88cbcc-922c-4e40-950b-51cda9445da2" />
7. client kirim pesan kosong (prompt muncul lagi, tidak crash, tidak dikirim)  
   <img width="515" height="186" alt="Screenshot 2026-05-03 193359" src="https://github.com/user-attachments/assets/e18119eb-26e6-46b4-9a9d-77cecda8ae5a" />  
8. salah satu client exit  
   TERMINAL CLIENT YANG KELUAR SERVER  
   <img width="439" height="40" alt="Screenshot 2026-05-03 193644" src="https://github.com/user-attachments/assets/c199fda2-ca46-41d6-be04-98a5f288640b" />
   TERMINAL CLIENT LAIN  
   <img width="343" height="23" alt="Screenshot 2026-05-03 193722" src="https://github.com/user-attachments/assets/c77d1158-db46-4366-87b8-985fb047627e" />
9. nama duplikat ditolak dan langsung meminta input nama lain lagi  
    <img width="602" height="97" alt="Screenshot 2026-05-03 193910" src="https://github.com/user-attachments/assets/af1b7b9d-a640-46e0-b31a-8723c8611ef7" />
10. error handling enter nama kosong  
    <img width="495" height="122" alt="Screenshot 2026-05-03 194024" src="https://github.com/user-attachments/assets/113d7834-b610-4685-b2e9-de95da0d2807" />
11. password The Knights salah  
    <img width="488" height="78" alt="Screenshot 2026-05-03 194118" src="https://github.com/user-attachments/assets/09fdfdb2-1fae-4476-9265-965c83f6a60c" />
12. output name=The Knights  
    <img width="620" height="422" alt="Screenshot 2026-05-03 194244" src="https://github.com/user-attachments/assets/91d305e2-f62e-46be-a1ae-c410f46c3a4f" />
    command 3 membuat server mati, command 4 tidak memunculkan output apapun tetapi The Knights keluar dari server & server dan client lain tetap jalan
13. error handling apabila input command The Knights selain 1,2,3,4  
    <img width="318" height="220" alt="Screenshot 2026-05-03 194557" src="https://github.com/user-attachments/assets/f9ed3a30-d8d5-43c4-ac03-a7e58e76b778" />  
14. isi history.log  
    <img width="588" height="404" alt="Screenshot 2026-05-03 194719" src="https://github.com/user-attachments/assets/9b65e7cc-111b-498a-9b78-1100391b855c" />

    

    
    





 




   
    
