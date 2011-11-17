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

int autoDrive(int act)
{
    struct sockaddr_un address;
    int sock;
    size_t addr_length;
    int len;
    int i;
    int sum = 0;

    char buf[512];

    // CGI to STM.
    unsigned char request_cmd[] = {0xF1, 0xF2, 0x08, 0x31, 0xA3, 0x01, act, 0xFF};
  
    for (i = 0; i < (sizeof(request_cmd) - 1); i++) {
	    sum += request_cmd[i];
    }
    sum &= 0xff;

    request_cmd[7] = sum;

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

    fprintf(cgiOut, "Written: %d.<br>\n", len);
    memset(buf, '\0', sizeof(buf));

    len = read(sock, buf, sizeof(buf)); // Read ACK.
    fprintf(cgiOut, "ACK read: %d.  [\n", len);
    for (i = 0; i < len; i++) {
        fprintf(cgiOut, "0x%02x ", buf[i] & 0xff);
    }

    fprintf(cgiOut, "]<br>");
    fprintf(cgiOut, "%s", "Done.\n");
    close(sock);

    return 0;
}

void handleSelect()
{
	int act = -1;
        char *action[] = {   
	    "ACTION_STOP",
	    "AUTO_ACTION",
	    "SPOT1_ACTION",
	    "SPOT2_ACTION",
	    "WALL_ACTION",
	    "PCI_ACTION",
        };

        int actionChoice;
    
        cgiFormSelectSingle("AutoAction", action, 6, &actionChoice, 0); 
        fprintf(cgiOut, "Select action: &nbsp;%s<BR>\n", action[actionChoice]);

	if (!strcmp(action[actionChoice], action[0])) {
		act = 0;
		fprintf(cgiOut, "運転停止<br>\n");
	} else if (!strcmp(action[actionChoice], action[1])) {
		act = 1;
		fprintf(cgiOut, "自動運転<br>\n");
	} else if (!strcmp(action[actionChoice], action[2])) {
		act = 2;
		fprintf(cgiOut, "SPOT1運転<br>\n");
	} else if (!strcmp(action[actionChoice], action[3])) {
		act = 3;
		fprintf(cgiOut, "SPOT2運転<br>\n");
	} else if (!strcmp(action[actionChoice], action[4])) {
		act = 4;
		fprintf(cgiOut, "壁ぎわ運転<br>\n");
	} else if (!strcmp(action[actionChoice], action[5])) {
		act = 5;
		fprintf(cgiOut, "PCI運転<br>\n");
	} 

	if (act != -1) {
		autoDrive(act);
	}	
}

int cgiMain()
{
	/* Send the content type, letting the browser know this is HTML */
	cgiHeaderContentType("text/html");
	fprintf(cgiOut, "<HTML><HEAD>\n");
	fprintf(cgiOut, "<TITLE>Robot AutoDrive</TITLE></HEAD>\n");
         
	if ((cgiFormSubmitClicked("send") == cgiFormSuccess)) {
            handleSelect();
	}

	fprintf(cgiOut, "</BODY></HTML>\n");
	return 0;
}


