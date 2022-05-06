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

int main(int argc, char* argv[]){

    char* str = "MY$$NAME$$IS$$JAKE";
    pid_t pid = getpid();
    char new[10];
    int j = 0;
    memset(new, '\0', 10);
    sprintf(new, "%d", pid);
    char fuck[100];
    memset(fuck, '\0', 100);
    for (int i = 0; i < strlen(str); i++){
        if (str[i] == '$'){
            if (str[i + 1] == '$')
                for (int k=0; k < strlen(new); k++){
                    fuck[j] = new[k];
                    j++;
                }
            i++;
        } else {
            fuck[j] = str[i];
            j++;
        }    
    }

    printf("%s\n", fuck);

    return EXIT_SUCCESS;
}