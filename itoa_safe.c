#define _BSD_SOURCE
#define _XOPEN_SOURCE 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/signal.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>


int main(){

    int status = 0;
    pid_t my_pid = getpid();  //using getpid for an example.
    //waitpid(-1, &status, WNOHANG); <------you would use this in signal handler. 
    char ltrs[10]; //size is arbitrary
    int leftovers = 0;
    int denominator = 10;
    int pid_copy = my_pid;
    int num_digits = 0;
    int i = 0;

    printf("PID: %d\n", my_pid); //using printf for demonstration only. Remove for signal handling.
    
    ltrs[num_digits] = '\0'; // to null terminate our future string
    if (pid_copy > 0){
        num_digits = floor(log10(abs(pid_copy))) + 1;
        for (i = 0; i < num_digits; i++){
            leftovers = pid_copy % denominator;
            pid_copy /= 10;
            leftovers += 48; // ascii conversion
            ltrs[(num_digits - 1) - i] = (char)leftovers;
        }
    }
    write(STDOUT_FILENO, ltrs, num_digits);
    return EXIT_SUCCESS;
}