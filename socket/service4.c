#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#include "config.h"

char *getSSID(void)
{
	return "COVIA";
}

void return_ssid(int sock, char *ssid)
{
//	struct sockaddr_un address;
//	int sock;
//	size_t addr_length;
	int len;
        int i;
        short sum = 0x00;

	unsigned char buf[512];
      
#if 0	
	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return;
	}

	address.sun_family = AF_UNIX;    /* Unix domain socket */
	strcpy(address.sun_path, SER4_DIS_SOCKET_FILE);

	/* The total length of the address includes the sun_family element */
	addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

	if (connect(sock, (struct sockaddr *) &address, addr_length)) {
		perror("connect");
		return;
	}
#endif

	buf[0] = 0xF1; 
	buf[1] = 0xF2; 
	buf[2] = 0xFF;  // command length
	buf[3] = 0x43; 
	buf[4] = 0xA0; 
	buf[5] = 0x02; 
					
	strncpy((char *)&(buf[6]), ssid, strlen(ssid));

        len = 6 + strlen(ssid);
	buf[2] = len + 1; // Last one value is checksum.
        for (i = 0; i < len; i++) {
                sum += buf[i];
	}
	buf[len] = sum & 0xff;

	printf("SSID response: ");
        for (i = 0; i < (len + 1); i++) {
		printf("0x%02x ", buf[i] & 0xff);
	}
	printf("\n");

	len = write(sock, buf, len + 1);
	printf("SSID(%s) written: %d. \n", ssid, len);

	memset(buf, '\0', sizeof(buf));
	len = read(sock, buf, sizeof(buf)); // Read ACK.
	printf("ACK read: len: %d\n", len);

//	printf("%s", "Close socket.\n");
//	close(sock);
}

void handle_cmd(int sock, unsigned char *buf)
{
        int cmd = ((buf[4] & 0xff) << 8) | (buf[5] & 0xff);
        switch (cmd) {
		case GET_WIFI_SSID: {
			char *ssid = getSSID();
			return_ssid(sock, ssid);
			break;
		}
	}
}

#if 0
void *dispatcher_incoming(void *ptr)
{
	struct sockaddr_un address;
	int sock, conn;
	socklen_t addr_length;
	int amount;
	fd_set ready;
	struct timeval to;
        char buf[1024];
        int i;
        int cmd;

        const unsigned char ack_cmd[] = {0xF1, 0xF2, 0x07, 0x23, 0x90, 0x03, 0xA0};

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	/* Remove any preexisting socket (or other file) */
	unlink(DIS_SER4_SOCKET_FILE);

	address.sun_family = AF_UNIX;       /* Unix domain socket */
	strcpy(address.sun_path, DIS_SER4_SOCKET_FILE);

	/* The total length of the address includes the sun_family
	   element */
	addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

	if (bind(sock, (struct sockaddr *) &address, addr_length)) {
		perror("bind");
		exit(1);
	}
 
        // Chmod for accessible to CGI.
	chmod(DIS_SER4_SOCKET_FILE, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | 
	                            S_IROTH | S_IWOTH | S_IXOTH);

	if (listen(sock, SOMAXCONN)) {  // 128
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
			printf("[Service-4] ---- getting data\n");

			memset(buf, '\0', sizeof(buf));
			amount = read(conn, buf, sizeof(buf)); // Dispatcher request.
			printf("[Service-4] Dispatcher request (len: %d): ", amount);
			for (i = 0; i < amount; i++) {
				printf("0x%02x ", buf[i] & 0xff);
			}
			printf("\n");

			// Must be someone waiting for reading else writing will be died.
			amount = write(conn, ack_cmd, sizeof(ack_cmd));
			close(conn);
			printf("Write amount(ACK): %d\n", amount);
			
			printf("[Service-4] ---- done\n");

			usleep(100000);
                        cmd = ((buf[4] & 0xff) << 8) | (buf[5] & 0xff);

			handle_cmd(cmd);
			
		} else {
			//printf("Do something else ...\n");
		}
	} while (1);

	close(sock);
}
#endif

void *conn(void *ptr)
{
    int sock;
    struct sockaddr_un address;
    size_t addr_length;
    int amount;
    fd_set ready_r;        
    struct timeval to;
    int i;

    unsigned char buf[256];

    const unsigned char register_cmd[] = {0xF1, 0xF2, 0x07, 0x40, 0x90, 0x01, 0xBB};
    const unsigned char ack_cmd[] = {0xF1, 0xF2, 0x07, 0x43, 0x90, 0x03, 0xC0};

    if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
	    perror("socket");
	    exit(1);
    }

    address.sun_family = AF_UNIX;    /* Unix domain socket */
    strcpy(address.sun_path, SER4_DIS_SOCKET_FILE);

    /* The total length of the address includes the sun_family element */
    addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

    if (connect(sock, (struct sockaddr *) &address, addr_length)) {
	    printf("Cannot register service. (%s)\n", strerror(errno));
	    exit(1);
    }

    amount = write(sock, register_cmd, sizeof(register_cmd));  // Register of service.
    amount = read(sock, buf, sizeof(buf));  // Read ACK from dispatcher.
    if (amount <= 0) {
	    printf("Cannot register service.\n");
	    exit(1);
    }

    //printf("Register done.\n");

    do {
	FD_ZERO(&ready_r);
	FD_SET(sock, &ready_r);
	to.tv_sec = 3;
	if (select(sock + 1, &ready_r, 0, 0, &to) < 0) {
		perror("select");
	}
	if (FD_ISSET(sock, &ready_r)) {
		memset(buf, '\0', sizeof(buf));
		amount = read(sock, buf, sizeof(buf)); // Dispatcher request.
		if (amount < 0) {
			perror("read");
			continue;
		}
		printf("[S4] Dispatcher request(%d): ", amount);
		for (i = 0; i < amount; i++) {
			printf("0x%02x ", buf[i] & 0xff);
		}
		printf("\n");

		amount = write(sock, ack_cmd, sizeof(ack_cmd));
		//close(sock);
		printf("ACK write amount: %d\n", amount);

		printf("[S4] ---- done\n");

		usleep(100000); // To wait ACK complete.

		handle_cmd(sock, buf);
	} 
    } while (1);

    close(sock);

    return 0;
}


void *bottom_incoming(void *ptr)
{
	while (1) {
		sleep(1);
	}
}

int main(int argc, char *argv[]) 
{
        int iret_conn, iret_bottom;
//        int iret_dispatch, iret_bottom;
//	pthread_t thread_dispatch;  // Accept for Dispatcher connection.
	pthread_t thread_bottom;    // Accept for Bottom connection.
	pthread_t thread_conn;

//        iret_dispatch = pthread_create(&thread_dispatch, NULL, dispatcher_incoming, (void *)NULL);
        iret_bottom = pthread_create(&thread_bottom, NULL, bottom_incoming, (void *)NULL);
        iret_conn = pthread_create(&thread_conn, NULL, conn, (void *)NULL);

//        pthread_join(thread_dispatch, NULL);
        pthread_join(thread_bottom, NULL); 
        pthread_join(thread_conn, NULL); 

//        printf("Thread dispatcher returns: %d\n", iret_dispatch);
        printf("Thread bottom returns: %d\n", iret_bottom);
        printf("Thread conn returns: %d\n", iret_conn);

        return 0;
}

