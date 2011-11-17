#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#include "cgic.h"
#include "robot.h"

int getMovingStatus(void)
{
    struct sockaddr_un address;
    int sock;
    size_t addr_length;
    int len;
    int i, status;

    char buf[512];

    // CGI to STM
    const unsigned char request_cmd[] = {0xF1, 0xF2, 0x07, 0x31, 0xA3, 0x04, 0xC2}; 
    const unsigned char ack_cmd[] = {0xF1, 0xF2, 0x07, 0x13, 0xA3, 0x04, 0xA4}; 

    if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        fprintf(cgiOut, "Create socket failed: %s.<br>\n", strerror(errno));
	return(1);
    }

    address.sun_family = AF_UNIX;    /* Unix domain socket */
    strcpy(address.sun_path, CGI_SOCKET_FILE);

    /* The total length of the address includes the sun_family
       element */
    addr_length = sizeof(address.sun_family) + strlen(address.sun_path);

    if (connect(sock, (struct sockaddr *) &address, addr_length)) {
        perror("connect");
        fprintf(cgiOut, "Connect failed: %s.<br>\n", strerror(errno) );
	return(1);
    }

    len = write(sock, request_cmd, sizeof(request_cmd));

    if (len < 0) {
	    perror("write"); 
            fprintf(cgiOut, "Write failed: %s.<br>\n", strerror(errno) );
	    return(1);
    }

    fprintf(cgiOut, "written: %d.<br>\n", len);
    memset(buf, '\0', sizeof(buf));

    len = read(sock, buf, sizeof(buf)); // Read ACK.
    fprintf(cgiOut, "ACK read: %d.  [\n", len);
    for (i = 0; i < len; i++) {
        fprintf(cgiOut, "0x%02x ", buf[i] & 0xff);
    }

    fprintf(cgiOut, "]<br>");

    len = read(sock, buf, sizeof(buf)); // Get Robot Moving Status.

    status = buf[6];

    len = write(sock, ack_cmd, sizeof(ack_cmd));
    fprintf(cgiOut, "ack written: %d.<br>\n", len);

    fprintf(cgiOut, "%s", "Close socket.\n");
    close(sock);

    return status;
}

int cgiMain()
{
        int status;

	/* Send the content type, letting the browser know this is HTML */
	cgiHeaderContentType("text/html");
	fprintf(cgiOut, "<HTML><HEAD>\n");
	fprintf(cgiOut, "<TITLE>Robot GetSsid</TITLE></HEAD>\n");

	status = getMovingStatus();
	fprintf(cgiOut, "<BODY><H1>Moving Status: %d</H1>\n", status);
	fprintf(cgiOut, "</BODY></HTML>\n");
	return 0;
}


