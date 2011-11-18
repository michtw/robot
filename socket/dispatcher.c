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
int service2_sock = -1;
int service4_sock = -1;
int stm_sock = -1;

int service2_register = 0;
int service4_register = 0;
int stm_register = 0;

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

int bypass2cgi(int conn, unsigned char *data, int len)
{
	int number;

	printf("%s: conn: %d\n", __func__, conn);
	assert(conn > 0);

	number = write(conn, data, len);  // Pass response to CGI.
	if (number < 0) {
		perror("write");
	}
	printf("Bypass response to CGI.  Written: %d\n", number);
	return 0;
}

/*
 *  Just bypass data here.
 */
int cgi2service(int cgi_sock, int service_sock, char *data, int len, char *serv_path) 
//int cgi2service(int cgi_sock, char *data, int len, char *serv_path) 
{
	//int sock;	
	//struct sockaddr_un address;
	//size_t addr_length;
//	fd_set ready;        
//	struct timeval to;
//        int i;
//        unsigned char buf[256];

	int number;

	assert(cgi_sock > 0);
	assert(service_sock > 0);

#if 0
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
#endif

	number = write(service_sock, data, len);  // Pass request command to service.
	printf("Pass request command to service. Written: %d\n", number);

#if 0  // read ACK from service_incoming()

	number = read(service_sock, buf, sizeof(buf));  // Read ACK from service.
	printf("Read ACK from service. read: %d\n", number);
	for (i = 0; i < number; i++) {
		printf("%02x ", buf[i]);
	}
	printf("\n");

#if 23
	FD_ZERO(&ready);
	FD_SET(cgi_sock, &ready);
	to.tv_sec = 3;
	//printf("Start write ACK. 3 seconds timeout.\n");
	if (select(cgi_sock + 1, 0, &ready, 0, &to) < 0) {
		perror("select");
	}
	if (FD_ISSET(cgi_sock, &ready)) {
	        number = write(cgi_sock, buf, number);  // Pass ACK to CGI.
	} else {
		printf("Cannot write ACK to CGI.\n");
	}

#endif

	if (number < 0) {
		perror("write");
	}
	printf("Pass ACK to CGI. Written: %d cgi_sock: %d\n", number, cgi_sock);

////	printf("%s", "Close socket.\n");
////	close(sock);
#endif

	return 0;
}

void *cgi_incoming(void *ptr)
{
	struct sockaddr_un address;
	int sock, conn;
	socklen_t addr_length;
	int amount;
	fd_set ready;
	struct timeval to;
        char buf[512];
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

	if (listen(sock, SOMAXCONN)) {
		perror("listen");
		exit(1);
	}

	do {
		FD_ZERO(&ready);
		FD_SET(sock, &ready);
		to.tv_sec = 5;
		if (select(sock + 1, &ready, 0, 0, &to) < 0) {
			//perror("select");
			continue;
		}

		if (FD_ISSET(sock, &ready)) {
			conn = accept(sock, (struct sockaddr *) &address, &addr_length);

                        cgi_sock = conn;

			if (conn == -1) {
				perror("accept");
				continue;
			}
			printf("---- Dispatcher getting data from CGI. cgi_sock: %d\n", cgi_sock);

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
					cgi2service(cgi_sock, service2_sock, buf, 
					            amount, DIS_SER2_SOCKET_FILE);
					break;
				case CGI2SERVICE4:
					printf("CGI 2 Service-4\n");
					cgi2service(cgi_sock, service4_sock, buf, 
					            amount, DIS_SER4_SOCKET_FILE);
					break;
				case CGI2STM:
					printf("CGI 2 STM\n");
					cgi2service(cgi_sock, stm_sock, buf, 
					            amount, DIS_STM_SOCKET_FILE);
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
	fd_set ready, ready2;
	struct timeval to;
        unsigned char buf[512];
        int i;

	const unsigned char register_ack[] = {0xF1, 0xF2, 0x07, 0x04, 0x90, 0x03, 0x81};

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

	conn = accept(sock, (struct sockaddr *) &address, &addr_length);
       	
        printf("conn: %d\n", conn);	
	if (conn < 0) {
		perror("accept");
	}

	service4_sock = conn;

	do {
		FD_ZERO(&ready);
		FD_SET(conn, &ready);
		to.tv_sec = 5;
		if (select(conn + 1, &ready, 0, 0, &to) < 0) {
			//perror("select");
			continue;
		}

		if (FD_ISSET(conn, &ready)) {

			printf("    ---- Dispatcher getting data from Service-4\n");

			memset(buf, '\0', sizeof(buf));
			amount = read(conn, buf, sizeof(buf)); // Service-2 request.
			printf("    Service-4 request(%d): ", amount);
			for (i = 0; i < amount; i++) {
				printf("0x%02x ", buf[i] & 0xff);
			}
			printf("\n");

                        if (((buf[4] << 8) | buf[5]) == REGISTER) {
				printf("Service-4 register.\n");
				service4_register = 1;
				amount = write(conn, register_ack, sizeof(register_ack));
				printf("Register ack written: %d\n", amount);
				continue;
			}

			switch (buf[3]) {
				case SERVICE42CGI:
					printf("Service-4 to CGI\n");
					bypass2cgi(cgi_sock, buf, amount);
					break;
			}

			printf("    ---- done\n");

			if (((buf[4] << 8) | buf[5]) == ACK) {
				continue; // No more data response.
			}
			
#if 23
			// Read service response data.
			FD_ZERO(&ready2);
			FD_SET(cgi_sock, &ready2);
			to.tv_sec = 5;
			if (select(cgi_sock + 1, &ready2, 0, 0, &to) < 0) {
				//perror("select");
				close(cgi_sock);
				continue;
			}
		        if (FD_ISSET(cgi_sock, &ready2)) {
				amount = read(cgi_sock, buf, sizeof(buf));  // Read ACK from CGI.
				close(cgi_sock);
				printf("Read ACK from CGI. amount: %d\n", amount);
			} else {
			        printf("Cannot get ACK from CGI.\n");
				close(cgi_sock);
                                // close(conn);
				continue;
			}
#endif
			amount = write(conn, buf, amount);     // Bypass ACK to Service-2.
			printf("Bypass ACK to service-4. Amount: %d\n", amount);
                        //// close(conn);
		} else {
			// printf("Do something else ...\n");
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
	fd_set ready2;
	struct timeval to;
        unsigned char buf[512];
        int i;

	const unsigned char register_ack[] = {0xF1, 0xF2, 0x07, 0x02, 0x90, 0x03, 0x7F};

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

	if (listen(sock, SOMAXCONN)) {
		perror("listen");
		exit(1);
	}
#if 23
	conn = accept(sock, (struct sockaddr *) &address, &addr_length);
       	
        printf("conn: %d\n", conn);	
	if (conn < 0) {
		perror("accept");
	}
	service2_sock = conn;

	do {
		FD_ZERO(&ready);
		FD_SET(conn, &ready);
		to.tv_sec = 5;
		if (select(conn + 1, &ready, 0, 0, &to) < 0) {
			//perror("select");
			continue;
		}

		if (FD_ISSET(conn, &ready)) {

			printf("    ---- Dispatcher getting data from Service-2\n");

			memset(buf, '\0', sizeof(buf));
			amount = read(conn, buf, sizeof(buf)); // Service-2 request.
			printf("    Service-2 request(%d): ", amount);
			for (i = 0; i < amount; i++) {
				printf("0x%02x ", buf[i] & 0xff);
			}
			printf("\n");

                        if (((buf[4] << 8) | buf[5]) == REGISTER) {
				printf("Service 2 register.\n");
				service2_register = 1;
				amount = write(conn, register_ack, sizeof(register_ack));
				printf("Register ack written: %d\n", amount);
				continue;
			}

			switch (buf[3]) {
				case SERVICE22CGI:
					printf("Service-2 to CGI\n");
					bypass2cgi(cgi_sock, buf, amount);
					break;
			}

			printf("    ---- done\n");
			if (((buf[4] << 8) | buf[5]) == ACK) {
				continue;
			}
			
#if 23
			FD_ZERO(&ready2);
			FD_SET(cgi_sock, &ready2);
			to.tv_sec = 2;
			//printf("Start read ACK. 2 seconds timeout.\n");
			if (select(cgi_sock + 1, &ready2, 0, 0, &to) < 0) {
				//perror("select");
				close(cgi_sock);
				continue;
			}
		        if (FD_ISSET(cgi_sock, &ready2)) {
				amount = read(cgi_sock, buf, sizeof(buf));  // Read ACK from CGI.
				close(cgi_sock);
				printf("Read ACK from CGI. amount: %d\n", amount);
			} else {
			        printf("Cannot get ACK from CGI.\n");
				close(cgi_sock);
                                close(conn);
				continue;
			}

			amount = write(conn, buf, amount);     // Bypass ACK to Service-2.
			printf("Bypass ACK to Service-2. amount: %d\n", amount);
#endif
                        //// close(conn);
		} else {
			// printf("Do something else ...\n");
		}
	} while (1);
#else
	do {
		FD_ZERO(&ready);
		FD_SET(sock, &ready);
		to.tv_sec = 5;
		if (select(sock + 1, &ready, 0, 0, &to) < 0) {
			//perror("select");
			continue;
		}

		if (FD_ISSET(sock, &ready)) {
			conn = accept(sock, (struct sockaddr *) &address, &addr_length);

			if (conn < 0) {
				perror("accept");
				continue;
			}

                        service2_sock = conn;

			printf("    ---- Dispatcher getting data from Service-2\n");

			memset(buf, '\0', sizeof(buf));
			amount = read(conn, buf, sizeof(buf)); // Service-2 request.
			printf("    Service-2 request(%d): ", amount);
			for (i = 0; i < amount; i++) {
				printf("0x%02x ", buf[i] & 0xff);
			}
			printf("\n");

                        if (((buf[4] << 8) | buf[5]) == REGISTER) {
				printf("Service 2 register.\n");
				service2_register = 1;
				amount = write(conn, register_ack, sizeof(register_ack));
				printf("Register written: %d\n", amount);
				continue;
			}

			switch (buf[3]) {
				case SERVICE22CGI:
					printf("Service-2 to CGI\n");
					bypass2cgi(cgi_sock, buf, amount);
					break;
			}

			printf("    ---- done\n");
			
#if 23
			FD_ZERO(&ready2);
			FD_SET(cgi_sock, &ready2);
			to.tv_sec = 2;
			printf("Start read ACK. 2 seconds timeout.\n");
			if (select(cgi_sock + 1, &ready2, 0, 0, &to) < 0) {
				//perror("select");
				close(cgi_sock);
				continue;
			}
		        if (FD_ISSET(cgi_sock, &ready2)) {
				amount = read(cgi_sock, buf, sizeof(buf));  // Read ACK from CGI.
				close(cgi_sock);
				printf("Read ACK from CGI. amount: %d\n", amount);
			} else {
			        printf("Cannot get ACK from CGI.\n");
				close(cgi_sock);
                                close(conn);
				continue;
			}
#endif

			amount = write(conn, buf, amount);     // Bypass ACK to Service-2.
			printf("Bypass ACK to Service-2. amount: %d\n", amount);
                        //// close(conn);
		} else {
			//printf("Do something else ...\n");
		}
	} while (1);
#endif

	close(sock);
}

void *stm_incoming(void *ptr)
{
	struct sockaddr_un address;
	int sock, conn;
	socklen_t addr_length;
	int amount;
	fd_set ready, ready2;
	struct timeval to;
        unsigned char buf[512];
        int i;
	const unsigned char register_ack[] = {0xF1, 0xF2, 0x07, 0x01, 0x90, 0x03, 0x7E};

	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	/* Remove any preexisting socket (or other file) */
	unlink(STM_DIS_SOCKET_FILE);

	address.sun_family = AF_UNIX;       /* Unix domain socket */
	strcpy(address.sun_path, STM_DIS_SOCKET_FILE);

	/* The total length of the address includes the sun_family element */
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

	conn = accept(sock, (struct sockaddr *) &address, &addr_length);
       	
        printf("conn: %d\n", conn);	
	if (conn < 0) {
		perror("accept");
	}

	stm_sock = conn;

	do {
		FD_ZERO(&ready);
		FD_SET(conn, &ready);
		to.tv_sec = 5;
		if (select(conn + 1, &ready, 0, 0, &to) < 0) {
			//perror("select");
			continue;
		}

		if (FD_ISSET(conn, &ready)) {

			printf("    ---- Dispatcher getting data from STM\n");

			memset(buf, '\0', sizeof(buf));
			amount = read(conn, buf, sizeof(buf)); // Service-2 request.
			printf("    STM request(%d): ", amount);
			for (i = 0; i < amount; i++) {
				printf("0x%02x ", buf[i] & 0xff);
			}
			printf("\n");

                        if (((buf[4] << 8) | buf[5]) == REGISTER) {
				printf("STM register.\n");
				stm_register = 1;
				amount = write(conn, register_ack, sizeof(register_ack));
				printf("Register ack written: %d\n", amount);
				continue;
			}

			switch (buf[3]) {
				case STM2CGI:
					printf("STM to CGI\n");
					bypass2cgi(cgi_sock, buf, amount);
					break;
			}

			printf("    ---- done\n");

			if (((buf[4] << 8) | buf[5]) == ACK) {
				continue; // No more data response.
			}
			
#if 23
			// Read service response data.
			FD_ZERO(&ready2);
			FD_SET(cgi_sock, &ready2);
			to.tv_sec = 2;
			if (select(cgi_sock + 1, &ready2, 0, 0, &to) < 0) {
				//perror("select");
				close(cgi_sock);
				continue;
			}
		        if (FD_ISSET(cgi_sock, &ready2)) {
				amount = read(cgi_sock, buf, sizeof(buf));  // Read ACK from CGI.
				close(cgi_sock);
				printf("Read ACK from CGI. amount: %d\n", amount);
			} else {
			        printf("Cannot get ACK from CGI.\n");
				close(cgi_sock);
                                close(conn);
				continue;
			}
#endif
			amount = write(conn, buf, amount);     // Bypass ACK to Service-2.
			printf("Bypass ACK to STM. amount: %d\n", amount);
                        //// close(conn);
		} else {
			// printf("Do something else ...\n");
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

