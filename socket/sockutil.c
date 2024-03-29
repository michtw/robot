
/* sockutil.c - Utility functions used in socket example programs */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//#include "sockutil.h"

/* issue an error message via perror() and terminate the program */
void die(char * message) 
{
    perror(message);
    exit(1);
}

/* Copies data from file descriptor 'from' to file descriptor
   'to' until nothing is left to be copied. Exits if an error
   occurs. This assumes both from and to are set for blocking
   reads and writes. */
void copyData(int from, int to) 
{
    char buf[1024];
    int amount;
   
    while ((amount = read(from, buf, sizeof(buf))) > 0) {
        if (write(to, buf, amount) != amount) {
            die("write");
            return;
        }
    }

    if (amount < 0) {
        die("read");
    }
}

