#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "cgic.h"
#include "robot.h"

int getName(char *name)
{
    struct sockaddr_un address;
    int sock;
    size_t addr_length;
    int len;
    int i;
    char buf[512];

    struct tm *local;
    time_t t;

    t = time(NULL);
    local = localtime(&t);
//    fprintf(cgiOut, "Local time and date: %s<BR>\n", asctime(local));

    // CGI to Service-2.
    const unsigned char request_cmd[] = {0xF1, 0xF2, 0x07, 0x32, 0xA1, 0x03, 0xC0};
    const unsigned char ack_cmd[] = {0xF1, 0xF2, 0x07, 0x32, 0x90, 0x03, 0xAF};

    if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket");
//        fprintf(cgiOut, "Create socket failed: %s.<br>\n", strerror(errno));
	return(1);
    }

    address.sun_family = AF_UNIX;    /* Unix domain socket */
    strcpy(address.sun_path, CGI_SOCKET_FILE);

    /* The total length of the address includes the sun_family
       element */
    addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

    if (connect(sock, (struct sockaddr *) &address, addr_length)) {
        perror("connect");
//        fprintf(cgiOut, "Connect failed: %s.<br>\n", strerror(errno) );
	return(1);
    }

    len = write(sock, request_cmd, sizeof(request_cmd));

    if (len < 0) {
	    perror("write"); 
            fprintf(cgiOut, "Write failed: %s.<br>\n", strerror(errno) );
	    return(1);
    }

//    fprintf(cgiOut, "Written: %d.<br>\n", len);
    memset(buf, '\0', sizeof(buf));

    len = read(sock, buf, sizeof(buf)); // Read ACK.
//    fprintf(cgiOut, "ACK read: %d.  [\n", len);
    for (i = 0; i < len; i++) {
//        fprintf(cgiOut, "0x%02x ", buf[i] & 0xff);
    }

//    fprintf(cgiOut, "]<br>");

    len = read(sock, buf, sizeof(buf)); // Read name.

    strncpy(name, &(buf[6]), len - 7); // Minus header length
//    printf("Name: (len: %d) \"%s\"<br>\n", len, name);

    len = write(sock, ack_cmd, sizeof(ack_cmd));
//    fprintf(cgiOut, "ACK write: %d.<BR>", len);

//    fprintf(cgiOut, "%s", "Close socket.\n");
    close(sock);

    return 0;
}


//        { 'robot' = [ {'name' : 'Bruce'}  ] }
int cgiMain()
{
	char name[128];
        int rtn;

	/* Send the content type, letting the browser know this is HTML */
	cgiHeaderContentType("text/html");
//	fprintf(cgiOut, "<HTML><HEAD>\n");
//	fprintf(cgiOut, "<TITLE>Robot GetName</TITLE></HEAD>\n");

	memset(name, '\0', sizeof(name));
	rtn = getName(name);
	if (rtn == 0) {
                char str[128];
                sprintf(str, "{\"name\":\"%s\"}", name);
	        fprintf(cgiOut, "%s", str);
	        //fprintf(cgiOut, "<BODY><H1>Name: %s</H1>\n", name);
        }
	//fprintf(cgiOut, "</BODY></HTML>\n");
	return 0;
}


