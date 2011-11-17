#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#include "cgic.h"
#include "robot.h"
#include "lib.h"

static int is_ack(unsigned char *buf)
{
    // {0xF1, 0xF2, 0x07, 0x23, 0x90, 0x03, 0xA0}	
    //  0xf1  0xf2  0x07  0x34  0xa0 0x02 0xc0
    if ((buf[0] == 0xF1) && (buf[1] == 0xF2) && (buf[2] == 0x07) && 
        (buf[3] == 0x23) && (buf[4] == 0x90) && (buf[5] == 0x03) && (buf[6] == 0xA0)) {
	    return 0;
    } else {
	    return 1;
    }
}

int getSsid(char *ssid)
{
    struct sockaddr_un address;
    int sock;
    size_t addr_length;
    int len;
    int i;

    unsigned char buf[128];

    const char ack_cmd[] = {0xF1, 0xF2, 0x07, 0x34, 0x90, 0x03, 0xB1};
    const unsigned char request_cmd[] = {0xF1, 0xF2, 0x07, 0x34, 0xA0, 0x02, 0xC0}; // CGI to Service-4.

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

    if (is_ack(buf)) {
        fprintf(cgiOut, "Command error.<BR>\n", len);
	// return 1;
    }

    fprintf(cgiOut, "ACK read: %d.  [\n", len);

    for (i = 0; i < len; i++) {
        fprintf(cgiOut, "0x%02x ", buf[i] & 0xff);
    }

    fprintf(cgiOut, "]<br>");

    len = read(sock, buf, sizeof(buf)); // Read Wi-Fi SSID.

    strncpy(ssid, &(buf[6]), len - 7); // minus header length
    printf("SSID read: (len: %d) \"%s\"<br>\n", len, ssid);


    len = write(sock, ack_cmd, sizeof(ack_cmd));
    fprintf(cgiOut, "ACK written: %d.<br>\n", len);

    fprintf(cgiOut, "%s", "Close socket.\n");
    close(sock);

    return 0;
}

int cgiMain()
{
	char ssid[128];
        int rtn;

	/* Send the content type, letting the browser know this is HTML */
	cgiHeaderContentType("text/html");
	fprintf(cgiOut, "<HTML><HEAD>\n");
	fprintf(cgiOut, "<TITLE>Robot GetSsid</TITLE></HEAD>\n");

	memset(ssid, '\0', sizeof(ssid));
	rtn = getSsid(ssid);
	if (rtn == 0) {
	        fprintf(cgiOut, "<BODY><H1>SSID: %s</H1>\n", ssid);
        }
	fprintf(cgiOut, "</BODY></HTML>\n");
	return 0;
}


