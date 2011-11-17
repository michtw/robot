#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "config.h"
#include "service2.h"

char wifi_name[128];

static void get_name(void)
{
    struct sockaddr_un address;
    int sock;
    size_t addr_length;
    int len;
    int i, idx = 0;
    short sum = 0x00;
 
    char buf[128];

    if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
	    perror("socket");
	    return;
    }

    address.sun_family = AF_UNIX;    /* Unix domain socket */
    strcpy(address.sun_path, SER2_DIS_SOCKET_FILE);

    /* The total length of the address includes the sun_family
       element */
    addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

    if (connect(sock, (struct sockaddr *) &address, addr_length)) {
	    perror("connect");
	    return;
    }

    buf[idx++] = 0xF1; 
    buf[idx++] = 0xF2; 
    buf[idx++] = 0x00;  // command length
    buf[idx++] = 0x23; 
    buf[idx++] = 0xA1; 
    buf[idx++] = 0x03; 

    strncpy(&(buf[idx]), wifi_name, strlen(wifi_name));

    idx += strlen(wifi_name);
    printf("Name: %s\n", wifi_name);

    buf[2] = 6 + strlen(wifi_name) + 1;

    for (i = 0; i < idx; i++) {
	    sum += buf[i];
    }
    buf[idx++] = sum & 0xff;

    printf("Get name response: ");
    for (i = 0; i < idx; i++) {
	    printf("0x%02x ", buf[i] & 0xff);
    }
    printf("\n");

    len = write(sock, buf, idx);
    printf("Get name. Written: %d. \n", len);

    memset(buf, '\0', sizeof(buf));
    len = read(sock, buf, sizeof(buf)); // Read ACK.
    printf("ACK read: len: %d\n", len);

    printf("%s", "Close socket.\n");
    close(sock);
}

static void set_name(char *name, int len)
{
    memset(wifi_name, '\0', sizeof(wifi_name));
    memcpy(wifi_name, name, len);
}

void handle_cmd(char *buf)
{
    int cmd = ((buf[4] & 0xff) << 8) | (buf[5] & 0xff);

    switch (cmd) {
	    case SP_GetName:
		    printf("GetName\n");
		    get_name();
		    break;
	    case SP_SetName: {
		    char name[64];
		    int len = buf[2];
                    memset(name, '\0', sizeof(name));
		    strncpy(name, &(buf[6]), len - 7); // Minus header and checksum.
		    printf("SetName. Name: %s\n", name);
		    set_name(name, len - 7);
	    }	    break;
	    default:
		    printf("[Service-2] ERROR: Command not support.\n");
		    break;
    }
}

void *dispatcher_incoming(void *ptr)
{
    struct sockaddr_un address;
    int sock, conn;
    socklen_t addr_length;
    int amount;
    fd_set ready;
    struct timeval to;
    char buf[512];
    int i;
    //int cmd;

    const char ack_cmd[] = {0xF1, 0xF2, 0x07, 0x32, 0x90, 0x03, 0xAF};

    if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
	    perror("socket");
	    exit(1);
    }

    /* Remove any preexisting socket (or other file) */
    unlink(DIS_SER2_SOCKET_FILE);

    address.sun_family = AF_UNIX;       /* Unix domain socket */
    strcpy(address.sun_path, DIS_SER2_SOCKET_FILE);

    /* The total length of the address includes the sun_family
       element */
    addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

    if (bind(sock, (struct sockaddr *) &address, addr_length)) {
	    perror("bind");
	    exit(1);
    }

    chmod(DIS_SER2_SOCKET_FILE, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | 
	                        S_IROTH | S_IWOTH | S_IXOTH);

    if (listen(sock, 5)) {
	    perror("listen");
	    exit(1);
    }

    do {
	    FD_ZERO(&ready);
	    FD_SET(sock, &ready);
	    to.tv_sec = 5;
	    if (select(sock + 1, &ready, 0, 0, &to) < 0) {
		    perror("select");
		    continue;
	    }

	    if (FD_ISSET(sock, &ready)) {
		    conn = accept(sock, (struct sockaddr *) &address, &addr_length);

		    if (conn < 0) {
			    perror("accept");
			    continue;
		    }
		    printf("[S2] ---- getting data\n");

		    memset(buf, '\0', sizeof(buf));
		    amount = read(conn, buf, sizeof(buf)); // CGI request.
		    printf("[S2] Dispatcher request(%d): ", amount);
		    for (i = 0; i < amount; i++) {
			    printf("0x%02x ", buf[i] & 0xff);
		    }
		    printf("\n");

		    amount = write(conn, ack_cmd, sizeof(ack_cmd));
		    close(conn);
		    printf("Write amount: %d\n", amount);

		    printf("[S2] ---- done\n");

		    usleep(100000);
		    //cmd = ((buf[4] & 0xff) << 8) | (buf[5] & 0xff);

		    handle_cmd(buf);
	    } else {
		    //printf("Do something else ...\n");
	    }
    } while (1);

    close(sock);
}

void *bottom_incoming(void *ptr)
{
	while (1) {
		sleep(1);
	}
}

int main(int argc, char *argv[]) 
{
        int iret_dispatcher, iret_bottom;
	pthread_t thread_dispatcher;  // Accept for Dispatcher connection.
	pthread_t thread_bottom;    // Accept for Bottom connection.

	memset(wifi_name, '\0', sizeof(wifi_name));
        memcpy(wifi_name, "Unknow", strlen("Unknow"));

        iret_dispatcher = pthread_create(&thread_dispatcher, NULL, dispatcher_incoming, (void *)NULL);
        iret_bottom = pthread_create(&thread_bottom, NULL, bottom_incoming, (void *)NULL);

        pthread_join(thread_dispatcher, NULL);
        pthread_join(thread_bottom, NULL); 


        printf("Thread dispatcher returns: %d\n", iret_dispatcher);
        printf("Thread bottom returns: %d\n", iret_bottom);

        return 0;
}

