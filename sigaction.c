/********************************************
 * Name: Jake Hathaway
 * Date: 5/4/2022
 * Description: Test for Signal Interrupts
********************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/signal.h>

void handle_SIGINT(int signo){
    const char* message = "Caught SIGINT, sleeping for 10 seconds.\n";
    write(STDOUT_FILENO, message, 39);
    sleep(10);

}

void initialize_sig_handlers(){
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = handle_SIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);
}


int main(){

    initialize_sig_handlers();
    printf("%s", "Send the signal SIGINT by pressing Control-C.\n");
    fflush(stdout);
    pause();
    printf("%s", "pause() ended, signal received.\n");
    return EXIT_SUCCESS;
}
