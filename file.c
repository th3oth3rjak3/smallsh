#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/signal.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(){

    char *command[] = { "grep", "-E", "c$", "-", 0 };
    char *bin_file = command[0];


    int redirect_fd = open("redirect_fd.txt", O_RDWR | O_CREAT | O_TRUNC);
    dup2(redirect_fd, STDOUT_FILENO);
    close(redirect_fd);

    if (execvp(bin_file, command) == -1){
        fprintf(stderr, "Error executing %s\n", bin_file);
    }

    printf("done!\n");

}
