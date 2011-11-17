/* userver.c - Simple Unix Domain Socket server */

/* Waits for a connection on the ./sample-socket Unix domain
   socket. Once a connection has been established, copy data
   from the socket to stdout until the other end closes the
   connection, and then wait for another connection to the
   socket. */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <assert.h>

#include "config.h"

int cgi_sock;

int checksum(char *buf)
{
	int len = buf[2];
	int i;
	int sum = 0;

	len--; // Minus checksum byte.
        for (i = 0; i < len; i++) {
		sum += buf[i];
	}
	sum &= 0xff;
/*
	printf("sum: %#x\n", sum);
	printf("buf[%d]: %#x\n", len, buf[len]);
 */
	if ((buf[len] & 0xff) != sum) {
	    return 1;
	} else {
	    return 0;
	}
}

int bypass2cgi(int conn, char *data, int len)
{
	int number;

	assert(conn > 0);

	number = write(conn, data, len);  // Pass response to CGI.
	printf("Bypass response to CGI.  Written: %d\n", number);
	return 0;
}

/*
 *  Just bypass data here.
 */
#if 23
int cgi2service(int cgi_sock, char *data, int len, char *serv_path) 
{
	int sock;
	struct sockaddr_un address;
	size_t addr_length;
	int number;
        char buf[256];

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return 1;
	}

	address.sun_family = AF_UNIX;    /* Unix domain socket */
	strcpy(address.sun_path, serv_path);

	/* The total length of the address includes the sun_family element */
	addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

	if (connect(sock, (struct sockaddr *) &address, addr_length)) {
		perror("connect");
		return 1;
	}

	number = write(sock, data, len);  // Pass request command to service.
	printf("Pass request command to service. Written: %d\n", number);

	number = read(sock, buf, sizeof(buf));  // Read ACK from service.
	printf("Read ACK from service. read: %d\n", number);

	number = write(cgi_sock, buf, number);  // Pass ACK to CGI.
	printf("Pass ACK to CGI. written: %d\n", number);

	printf("%s", "Close socket.\n");
	close(sock);

	return 0;
}
#else
int cgi2service2(int conn, char *data, int len) 
{
	struct sockaddr_un address;
	int sock;
	size_t addr_length;
	int number;
        char buf[1024];

	assert(conn > 0);

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return 1;
	}

	address.sun_family = AF_UNIX;    /* Unix domain socket */
	strcpy(address.sun_path, DIS_SER2_SOCKET_FILE);

	/* The total length of the address includes the sun_family
	   element */
	addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

	if (connect(sock, (struct sockaddr *) &address, addr_length)) {
		perror("connect");
		return 1;
	}

	number = write(sock, data, len);  // Pass request command to service-2.
	printf("written: %d\n", number);
	number = read(sock, buf, sizeof(buf));  // Read ACK from service-2.
	printf("read: %d\n", number);

	number = write(conn, data, len);  // Pass ACK to CGI.
	printf("Pass ACK to CGI. written: %d\n", number);
	printf("%s", "Close socket.\n");
	close(sock);

	return 0;
}

int cgi2service4(int conn, char *data, int len) 
{
	struct sockaddr_un address;
	int sock;
	size_t addr_length;
	int number;
        char buf[1024];

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return 1;
	}

	address.sun_family = AF_UNIX;    /* Unix domain socket */
	strcpy(address.sun_path, DIS_SER4_SOCKET_FILE);

	/* The total length of the address includes the sun_family
	   element */
	addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

	if (connect(sock, (struct sockaddr *) &address, addr_length)) {
		perror("connect");
		return 1;
	}

	number = write(sock, data, len);  // Pass request command to service-4.
	printf("Pass request command to service-4. Written: %d\n", number);

	number = read(sock, buf, sizeof(buf));  // Read ACK from service-4.
	printf("Read ACK from service-4. read: %d\n", number);

	number = write(conn, data, len);  // Pass ACK to CGI.
	printf("Pass ACK to CGI. written: %d\n", number);

	printf("%s", "Close socket.\n");
	close(sock);

	return 0;
}

int cgi2stm(int conn, char *data, int len)
{
	struct sockaddr_un address;
	int sock;
	size_t addr_length;
	int number;
        char buf[1024];

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return 1;
	}

	address.sun_family = AF_UNIX;    /* Unix domain socket */
	strcpy(address.sun_path, DIS_STM_SOCKET_FILE);

	/* The total length of the address includes the sun_family
	   element */
	addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

	if (connect(sock, (struct sockaddr *) &address, addr_length)) {
		perror("connect");
		return 1;
	}

	number = write(sock, data, len);  // Pass request command to STM service.
	printf("[STM] Pass CGI request to STM. Written: %d\n", number);

	number = read(sock, buf, sizeof(buf));  // Read ACK from STM.
	printf("[STM] Read ACK from STM. Read: %d\n", number);

	number = write(conn, data, len);  // Pass ACK to CGI.
	printf("[STM] Pass ACK to CGI. Written: %d\n", number);

	printf("%s", "[STM] Close socket.\n");
	close(sock);

	return 0;
}
#endif

void *cgi_incoming(void *ptr)
{
	struct sockaddr_un address;
	int sock, conn;
	socklen_t addr_length;
	int amount;
	fd_set ready;
	struct timeval to;
        char buf[1024];
        int i;
        int path;

        //const char ack_cmd[] = {0xF1, 0xF2, 0x07, 0x13, 0x90, 0x03, 0x90};
        const char nack_checksum_err[] = {0xF1, 0xF2, 0x08, 0x03, 0x90, 0x04, 0x06, 0x88};

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	/* Remove any preexisting socket (or other file) */
	unlink(CGI_SOCKET_FILE);

	address.sun_family = AF_UNIX;       /* Unix domain socket */
	strcpy(address.sun_path, CGI_SOCKET_FILE);

	/* The total length of the address includes the sun_family
	   element */
	addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

	if (bind(sock, (struct sockaddr *) &address, addr_length)) {
		perror("bind");
		exit(1);
	}

	chmod(CGI_SOCKET_FILE, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | 
                               S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
     /*
	I second using SOMAXCONN, unless you have a specific reason to use a short queue.

	Keep in mind that if there is no room in the queue for a new connection, 
	no RST will be sent, allowing the client to automatically continue trying 
	to connect by retransmitting SYN.

	Also, the backlog argument can have different meanings in different socket implementations.

	In most it means the size of the half-open connection queue, 
	in some it means the size of the completed connection queue.
	In many implementations, the backlog argument will multiplied to yield a different queue length.
	If a value is specified that is too large, all implementations will 
	silently truncate the value to maximum queue length anyways.

      */

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

                        cgi_sock = conn;

			if (conn == -1) {
				perror("accept");
				continue;
			}
			printf("---- Dispatcher getting data from CGI\n");

			memset(buf, '\0', sizeof(buf));
			amount = read(conn, buf, sizeof(buf)); // CGI request.
			
			if (checksum(buf)) {
				printf("Command checksum error.\n");
				// Send NACK package.
                                write(conn, nack_checksum_err, sizeof(nack_checksum_err));
				continue;
			}

			printf("CGI request(%d): ", amount);
			for (i = 0; i < amount; i++) {
				printf("0x%02x ", buf[i] & 0xff);
			}
			printf("\n");

			path = buf[3];

			switch (path) {
				case CGI2SERVICE2:
					printf("CGI 2 Service-2\n");
					cgi2service(conn, buf, amount, DIS_SER2_SOCKET_FILE);
					break;
				case CGI2SERVICE4:
					printf("CGI 2 Service-4\n");
					cgi2service(conn, buf, amount, DIS_SER4_SOCKET_FILE);
					break;
				case CGI2STM:
					printf("CGI 2 STM\n");
					cgi2service(conn, buf, amount, DIS_STM_SOCKET_FILE);
					break;
				default:
					printf("0x%x: Wrong data format.\n", path);
					break;
			}

			printf("---- done\n");
			/////////////////////// close(conn);
		} else {
			//printf("Do something else ...\n");
		}
	} while (1);

	close(sock);
}

void *service4_incoming(void *ptr)
{
	struct sockaddr_un address;
	int sock, conn;
	socklen_t addr_length;
	int amount;
	fd_set ready;
	struct timeval to;
        char buf[1024];
        int i;

        //const char ack_cmd[] = {0xF1, 0xF2, 0x07, 0x13, 0x90, 0x03, 0x90};

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	/* Remove any preexisting socket (or other file) */
	unlink(SER4_DIS_SOCKET_FILE);

	address.sun_family = AF_UNIX;       /* Unix domain socket */
	strcpy(address.sun_path, SER4_DIS_SOCKET_FILE);

	/* The total length of the address includes the sun_family
	   element */
	addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

	if (bind(sock, (struct sockaddr *) &address, addr_length)) {
		perror("bind");
		exit(1);
	}

	chmod(SER4_DIS_SOCKET_FILE, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | 
                                    S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);

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
			printf("    ---- Dispatcher getting data from Service-4\n");

			memset(buf, '\0', sizeof(buf));
			amount = read(conn, buf, sizeof(buf)); // Service-4 request.
			printf("    Service-4 request(%d): ", amount);
			for (i = 0; i < amount; i++) {
				printf("0x%02x ", buf[i] & 0xff);
			}
			printf("\n");

			switch (buf[3]) {
				case SERVICE42CGI:
					printf("Service-4 to CGI\n");
					bypass2cgi(cgi_sock, buf, amount);
					break;
			}

			printf("    ---- done\n");
			
			amount = read(cgi_sock, buf, sizeof(buf));  // Read ACK from CGI.
			close(cgi_sock);
                        printf("Read ACK from CGI. amount: %d\n", amount);

			amount = write(conn, buf, amount);     // Bypass ACK to Service-4.
			printf("Bypass ACK to Service-4. amount: %d\n", amount);
                        close(conn);
		} else {
			//printf("Do something else ...\n");
		}
	} while (1);

	close(sock);
}

void *service2_incoming(void *ptr)
{
	struct sockaddr_un address;
	int sock, conn;
	socklen_t addr_length;
	int amount;
	fd_set ready;
	struct timeval to;
        char buf[512];
        int i;

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	/* Remove any preexisting socket (or other file) */
	unlink(SER2_DIS_SOCKET_FILE);

	address.sun_family = AF_UNIX;       /* Unix domain socket */
	strcpy(address.sun_path, SER2_DIS_SOCKET_FILE);

	/* The total length of the address includes the sun_family
	   element */
	addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

	if (bind(sock, (struct sockaddr *) &address, addr_length)) {
		perror("bind");
		exit(1);
	}

	chmod(SER2_DIS_SOCKET_FILE, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | 
                                    S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);

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
			printf("    ---- Dispatcher getting data from Service-2\n");

			memset(buf, '\0', sizeof(buf));
			amount = read(conn, buf, sizeof(buf)); // Service-4 request.
			printf("    Service-2 request(%d): ", amount);
			for (i = 0; i < amount; i++) {
				printf("0x%02x ", buf[i] & 0xff);
			}
			printf("\n");

			switch (buf[3]) {
				case SERVICE22CGI:
					printf("Service-2 to CGI\n");
					bypass2cgi(cgi_sock, buf, amount);
					break;
			}

			printf("    ---- done\n");
			
			amount = read(cgi_sock, buf, sizeof(buf));  // Read ACK from CGI.
			close(cgi_sock);
                        printf("Read ACK from CGI. amount: %d\n", amount);

			amount = write(conn, buf, amount);     // Bypass ACK to Service-2.
			printf("Bypass ACK to Service-2. amount: %d\n", amount);
                        close(conn);
		} else {
			//printf("Do something else ...\n");
		}
	} while (1);

	close(sock);
}

void *stm_incoming(void *ptr)
{
	struct sockaddr_un address;
	int sock, conn, path;
	socklen_t addr_length;
	int amount;
	fd_set ready;
	struct timeval to;
        char buf[1024];
        int i;

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	/* Remove any preexisting socket (or other file) */
	unlink(STM_DIS_SOCKET_FILE);

	address.sun_family = AF_UNIX;       /* Unix domain socket */
	strcpy(address.sun_path, STM_DIS_SOCKET_FILE);

	/* The total length of the address includes the sun_family
	   element */
	addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

	if (bind(sock, (struct sockaddr *) &address, addr_length)) {
		perror("bind");
		exit(1);
	}

	chmod(STM_DIS_SOCKET_FILE, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | 
                                   S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);

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
			printf("    ---- Dispatcher getting data from STM\n");

			memset(buf, '\0', sizeof(buf));
			amount = read(conn, buf, sizeof(buf)); // Service-4 request.
			printf("    STM request(%d): ", amount);
			for (i = 0; i < amount; i++) {
				printf("0x%02x ", buf[i] & 0xff);
			}
			printf("\n");

			path = buf[3];

			switch (path) {
				case STM2CGI:
					printf("STM to CGI\n");
					bypass2cgi(cgi_sock, buf, amount);
					break;
			}

			printf("    ---- done\n");
			
			amount = read(cgi_sock, buf, sizeof(buf));  // Read ACK from CGI.
			close(cgi_sock);
                        printf("Read ACK from CGI. amount: %d\n", amount);

			amount = write(conn, buf, amount);     // Bypass ACK to STM.
			printf("Bypass ACK to STM. Amount: %d\n", amount);
                        close(conn);
		} else {
			//printf("Do something else ...\n");
		}
	} while (1);

	close(sock);
}

int main(int argc, char *argv[]) 
{
        int iret_cgi, iret_stm, iret_s4, iret_s2;
	pthread_t thread_cgi;  // Accept for CGI connection.
	pthread_t thread_stm;  // Accept for STM connection.
	pthread_t thread_s4;   // Accept for Service-4 connection. (Media service)
	pthread_t thread_s2;   // Accept for Service-2 connection. (ARM11 service)


        iret_cgi = pthread_create(&thread_cgi, NULL, cgi_incoming, (void *)NULL);
        iret_stm = pthread_create(&thread_stm, NULL, stm_incoming, (void *)NULL);
        iret_s4 = pthread_create(&thread_s4, NULL, service4_incoming, (void *)NULL);
        iret_s2 = pthread_create(&thread_s2, NULL, service2_incoming, (void *)NULL);

        pthread_join(thread_cgi, NULL);
        pthread_join(thread_stm, NULL); 
        pthread_join(thread_s4, NULL); 
        pthread_join(thread_s2, NULL); 

        printf("Thread CGI returns: %d\n", iret_cgi);
        printf("Thread STM returns: %d\n", iret_stm);
        printf("Thread Service-4 returns: %d\n", iret_s4);
        printf("Thread Service-2 returns: %d\n", iret_s2);

        return 0;
}
