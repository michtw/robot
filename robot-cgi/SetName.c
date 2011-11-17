#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#include "cgic.h"
#include "robot.h"
#include "stm.h"

int setName(char *name)
{
    struct sockaddr_un address;
    int sock;
    size_t addr_length;
    int len;
    int i;
    int sum = 0;

    char buf[512];

    // CGI to Service-2.
    char request_cmd[128] = {0xF1, 0xF2, 0x00, 0x32, 0xA1, 0x04, };
  
    strncpy(&(request_cmd[6]), name, strlen(name));

    len = 6 + strlen(name);

    request_cmd[2] = len + 1;

    for (i = 0; i < len; i++) {
	    sum += request_cmd[i];
    }
    sum &= 0xff;

    request_cmd[len] = sum;

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

    len = write(sock, request_cmd, len + 1);

    if (len < 0) {
	    perror("write"); 
            fprintf(cgiOut, "Write failed: %s.<br>\n", strerror(errno) );
	    return(1);
    }

//    fprintf(cgiOut, "Written: %d.<br>\n", len);
    memset(buf, '\0', sizeof(buf));

    len = read(sock, buf, sizeof(buf)); // Read ACK.
    fprintf(cgiOut, "ACK read: (%d)  &nbsp;&nbsp; [\n", len);
    for (i = 0; i < len; i++) {
        fprintf(cgiOut, "0x%02x ", buf[i] & 0xff);
    }

    fprintf(cgiOut, "]<br>");
    fprintf(cgiOut, "%s", "Done.\n");
    close(sock);

    return 0;
}

void handlePlainText()
{
    char value[32];
    memset(value, '\0', sizeof(value));
    cgiFormStringNoNewlines("name", value, 32);
    fprintf(cgiOut, "<BR>Name: <B><font color=\"blue\">");
    cgiHtmlEscape(value);
    fprintf(cgiOut, "</font></B><BR>\n");
    setName(value);
}

int cgiMain()
{
	/* Send the content type, letting the browser know this is HTML */
	cgiHeaderContentType("text/html");
	fprintf(cgiOut, "<HTML><HEAD>\n");
	fprintf(cgiOut, "<TITLE>Robot SetName</TITLE></HEAD>\n");
         
	if ((cgiFormSubmitClicked("send") == cgiFormSuccess)) {
            handlePlainText();
	}

	fprintf(cgiOut, "</BODY></HTML>\n");
	return 0;
}


