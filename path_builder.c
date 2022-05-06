/********************************************
 * Name: Jake Hathaway
 * Date: 5/4/2022
 * Description: Test for Environment Vars
********************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/signal.h>

extern char** environ;

void pathBuilder(char* input_path){

    char path[256];
    if(strncmp(input_path, "./", 2) == 0){
        getcwd(path, 256);
    }
    printf("%s\n", path);
}

int main(int argc, char* argv[]){

    

    if (argc == 2){
        char* user_path = argv[1];
        chdir(argv[1]);
        char buff[255];
        memset(buff, '\0', 255);
        getcwd(buff, 255);
        printf("%s\n", buff);
        //printf("%s\n", pathBuilder(user_path));
        exit(EXIT_SUCCESS);
    } else {
        fprintf(stderr, "Usage: %s [PATH]\n", argv[0]);
    }

    return EXIT_SUCCESS;
}