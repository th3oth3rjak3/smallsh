

/****************************************************************
 * Name: Jake Hathaway
 * Date: 5/4/2022
 * Description: smallsh.c is a small shell program that can run
 * foreground processes, background processes, handles the
 * exec() family of functions, and implements builtin functions
 * such as exit, cd, and status.
****************************************************************/

#define _BSD_SOURCE
#define _XOPEN_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>

#define CHILDREN_MAX 20
#define COMMAND_LEN_MAX 2048
#define COMMAND_ARG_MAX 512
#define CMD_DELIMITER " "
#define PID_MAX_CHARS 10
#define PARENT_PROCESS 0
#define FG_CHILD_PROCESS 1
#define BG_CHILD_PROCESS 2
#define SMALL_BUFFER 255
#define BG_MODE_ON 1
#define BG_MODE_OFF 0
#define REDIRECT_OUTPUT_ON 1
#define REDIRECT_OUTPUT_OFF 0
#define REDIRECT_INPUT_ON 1
#define REDIRECT_INPUT_OFF 0
#define LOCAL_FUNCTION 0
#define EXEC_FUNCTION 1
#define MAX_PATH 255
#define NULL_CHAR '\0'
#define NEWLINE '\n'


/****************************************************************
 *                   Global Variables
 *
 * Global variables are only meant to be used during very
 * specific signal handling actions.
 *
 * gbl_BG_MODE: is a flag used to set if background commands are
 * allowed. It is set to 1 by default to allow background
 * commands. It will be set to 0 if a SIGTSTP is sent to the
 * parent process. It will be set back to 1 if a subsequent
 * SIGTSTP signal is sent to the parent process. Child processes
 * will not use this.
 *
 * gbl_FG_PROCESS: Used to house the PID of the current child
 * process running in the foreground. If one does not exist, it
 * is set to -1
 *
 * gbl_FG_STATUS: The exit status of the last FG process. By
 * default, it's set to 0.
****************************************************************/
int gbl_BG_MODE = 1;
int gbl_FG_PROCESS = -1;
int gbl_FG_STATUS = 0;

/****************************************************************
 *                        fail
 *
 * This function helps Jake not have to type so many fail
 * messages. If p_error is 1, perror() will be used to print the
 * error message. Useful when errno is set. If terminate is set
 * to 1, then the program will exit.
****************************************************************/

void fail(char* msg, int p_error, int terminate){
    if (p_error == 1){
        perror(msg);
    } else if (p_error == 0){
        fprintf(stderr, "Error: %s\n", msg);
    }
    if (terminate){
        exit(EXIT_FAILURE);
    } else {
        fflush(stdout);
        fflush(stderr);
    }
}

/****************************************************************
 *                        child_handler
 *
 * This is a function that is called whenever waitpid returns a
 * child pid. It prints messages to the screen for background
 * process completion.
****************************************************************/

void child_handler(pid_t pid, int status, int background_mode){


    char pid_str[PID_MAX_CHARS]={0};
    sprintf(pid_str, "%d", pid);
    if (WIFEXITED(status)){
        if(background_mode == BG_MODE_ON) {
            fprintf(stdout, "PID %s finished with exit status: %d\n", pid_str, WEXITSTATUS(status));
            fflush(stdout);
        } else {
            if (WEXITSTATUS(status) != 0){
                fprintf(stdout, "Exit status: %d\n", WEXITSTATUS(status));
                fflush(stdout);
            }
        }
    } else if (WIFSIGNALED(status)){
        if (background_mode == BG_MODE_ON){
            fprintf(stdout, "PID %s terminated by signal: %d\n", pid_str, WTERMSIG(status));
            fflush(stdout);
        } else {
            fprintf(stdout, "Terminated by signal: %d\n", pid_str, WTERMSIG(status));
            fflush(stdout);
        }

    }
}

/****************************************************************
 *                        handle_SIGINT
 *
 * This signal handler catches the SIGINT signal and prints the
 * termination signal to STDOUT.
 *
****************************************************************/

void handle_SIGINT(int sig){
    if (sig == SIGINT){
        char* msg = "\nTerminated by signal: 2\n";
        write(STDOUT_FILENO, msg, 25);
        fflush(stdout);
    }
}

/****************************************************************
 *                        parent_SIGTSTP
 * This signal handler catches the SIGTSTP signal and prints a
 * message to STDOUT.
****************************************************************/

void parent_SIGTSTP(){
    // todo: print a useful message, don't allow background commands anymore
    // until the user sends another SIGTSTP
    errno = 0;
    char msg[SMALL_BUFFER];
    memset(msg, NULL_CHAR, SMALL_BUFFER);
    char * outmsg;
    int len = 0;
    if (gbl_BG_MODE == 1){
        outmsg = "\nBackground Mode Disabled\n:";
        len = 27;
        gbl_BG_MODE = 0;
    } else if (gbl_BG_MODE == 0){
        outmsg = "\nBackground Mode Enabled\n:";
        len = 26;
        gbl_BG_MODE = 1;
    }
    write(1, outmsg, len);
}

void fg_child_SIGINT(){
    // todo: help a child end itself when SIGINT is sent to foreground
}

/****************************************************************
 *                   initialize_sig_handlers
 * This function defines and initializes the signal handlers
 * needed for the parent process. Child processes should redefine
 * the sigaction as needed.
****************************************************************/

void sig_handlers(int proc_type){
    if (proc_type == PARENT_PROCESS){
        //struct sigaction SIGTSTP_custom = {0};

        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, parent_SIGTSTP);
        //signal(SIGCHLD, SIG_IGN); // todo: SIGCHLD, do_something
    }
    if (proc_type == FG_CHILD_PROCESS){
        signal(SIGINT, fg_child_SIGINT);
        signal(SIGTSTP, SIG_IGN);
    }
    if (proc_type == BG_CHILD_PROCESS){
        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
    }
}

/****************************************************************
 *                        local_cd
 *
 * This function is a local implementation of the cd function to
 * change the current working directory. It takes zero or one
 * argument(s). If no argument is provided, then it will cd to
 * HOME directory. Otherwise it will change the directory to
 * the path provided if it's valid. Returns 0 for success, 1 for
 * failure.
****************************************************************/

int local_cd(int argc, char** argv){

    if (argc == 2){
        if (chdir(argv[1]) < 0){
            fail("cd", 1, 0);
            return EXIT_FAILURE;
        }
    } else if (argc == 1){
        if (chdir(getenv("HOME")) != 0) {
            fail("chdir", 1, 0);
            return EXIT_FAILURE;
        }
    } else {
        fail("Usage: ./smallsh [PATH]", 0, 0);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/****************************************************************
 *                        local_status
 *
 * This function is a local implementation that returns the
 * exit status of the last foreground child process to run.
****************************************************************/

int local_status(int status){
    if (WIFEXITED(status)){
        fprintf(stdout, "Exit status: %d\n", WEXITSTATUS(status));
        fflush(stdout);
    } else if (WIFSIGNALED(status)){
        fprintf(stdout, "Terminated by signal: %d\n", WTERMSIG(status));
        fflush(stdout);
    }

    return EXIT_SUCCESS;
}

/****************************************************************
 *                        exec_me
 *
 * This function uses the standard exec functions to execute
 * commands on the smallsh terminal. It will support input and
 * output redirection. It will also create background child
 * processes when specified.
****************************************************************/

int exec_me(char *argys[], int process_type, char input_redirection_path[], char output_redirection_path[],
            int input_redirection, int output_redirection, int* fg_status, int death_note[]){

    pid_t temp_pid;
    int fd0;
    int fd1;
    char *modified_input_path;
    char *modified_output_path;
    char input_path_array[MAX_PATH];
    char output_path_array[MAX_PATH];
    memset(input_path_array, NULL_CHAR, MAX_PATH);
    memset(output_path_array, NULL_CHAR, MAX_PATH);


    if(input_redirection == REDIRECT_INPUT_ON) {
        if (strncmp(input_redirection_path, "./", 2) != 0 && strncmp(input_redirection_path, "/", 1) != 0) {
            strcpy(input_path_array, "./");
            strcat(input_path_array, input_redirection_path);
            modified_input_path = input_path_array;
            if (open(modified_input_path, O_RDONLY) == -1){
                errno = ENOENT;
                fail(modified_input_path, 1, 0);
            }
        } else {
            modified_input_path = input_redirection_path;
        }
    }

    if(output_redirection == REDIRECT_OUTPUT_ON){
        if (strncmp(output_redirection_path, "./", 2) != 0 && strncmp(output_redirection_path, "/", 1) != 0) {
            strcpy(output_path_array, "./");
            strcat(output_path_array, output_redirection_path);
            modified_output_path = output_path_array;
        } else {
            modified_output_path = output_redirection_path;
        }
    }


    temp_pid = fork();
    switch (temp_pid){
        case -1:
            //error
            fail("fork", 1, 1);
        case 0:
            /* Be childish */

            //printf("child PID: %d\n", getpid());
            //fflush(stdout);
            if (process_type == BG_CHILD_PROCESS){
                sig_handlers(BG_CHILD_PROCESS);
            } else {
                sig_handlers(FG_CHILD_PROCESS);
            }

            if (input_redirection == REDIRECT_INPUT_ON){
                fd0 = open(modified_input_path, O_RDONLY);
                dup2(fd0, STDIN_FILENO);
                close(fd0);
            }
            if (output_redirection == REDIRECT_OUTPUT_ON) {
                fd1 = open(modified_output_path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                dup2(fd1, STDOUT_FILENO);
                close(fd1);
            }


            execvp(argys[0], argys);
            exit(EXIT_FAILURE);

        default:
            //parent
            sig_handlers(PARENT_PROCESS);
            if (process_type == FG_CHILD_PROCESS || gbl_BG_MODE == BG_MODE_OFF){
                waitpid(temp_pid, fg_status, 0);
                int temp = *fg_status;
                if (WIFEXITED(temp)){
                    if (WEXITSTATUS(temp) != 0){
                        errno = EINVAL;
                        fail(argys[0], 1, 0);
                    }
                } else if (WIFSIGNALED(temp)){
                    errno = EINVAL;
                    fail(argys[0], 1, 0);
                }

            } else {
                fprintf(stdout, "Background PID: %d\n", temp_pid);
                fflush(stdout);
                for (int i = 0; i < CHILDREN_MAX; i++){
                    //todo test pile of children in death note.
                    if (death_note[i] == 0){
                        death_note[i] = temp_pid;
                        break;
                    }
                }
            }
    }

    return EXIT_SUCCESS;
}

/****************************************************************
 *                        get_input
 *
 * This function collects input from the user on stdin and
 * converts it to an array of words. It returns the total count
 * of arguments from the user. It also handles inline expansion
 * of the $$ symbol to the terminal process ID. Finally, it also
 * determines if a & symbol is the last entry in the command
 * input. If so, it sets bg_proc to BG_CHILD_PROCESS.
****************************************************************/

int get_input(char *command_words[], int *command_counter, int *background_mode, char *input_path,
              char *output_path, int *input_redirect, int *output_redirect){

    char input_buffer[COMMAND_LEN_MAX];
    memset(input_buffer, NULL_CHAR, COMMAND_LEN_MAX);
    char command_string[COMMAND_LEN_MAX];
    memset(command_string, NULL_CHAR, COMMAND_LEN_MAX);
    fgets(input_buffer, COMMAND_LEN_MAX, stdin);
    size_t outer_counter;
    size_t input_length = strlen(input_buffer);
    size_t inner_counter;
    size_t counter;
    pid_t pid = getpid();
    char pid_str[PID_MAX_CHARS];
    sprintf(pid_str, "%d", pid);
    char output_redirection_path[MAX_PATH] = {0};
    char input_redirection_path[MAX_PATH] = {0};
    memset(output_redirection_path, NULL_CHAR, MAX_PATH);
    memset(input_redirection_path, NULL_CHAR, MAX_PATH);
    char *word;

    *background_mode = BG_MODE_OFF;
    *input_redirect = REDIRECT_INPUT_OFF;
    *output_redirect = REDIRECT_OUTPUT_OFF;
    *command_counter = 0;
    inner_counter = 0;

    strcpy(input_redirection_path, input_path);
    strcpy(output_redirection_path, output_path);

    for (outer_counter = 0; outer_counter < input_length; outer_counter++){
        if (input_buffer[outer_counter] == NEWLINE){
            command_string[inner_counter] = NULL_CHAR;
            inner_counter++;
        } else if (input_buffer[outer_counter] == '>' && outer_counter < (input_length - 1) &&
                   input_buffer[outer_counter + 1] == ' '){

            outer_counter++;
            outer_counter++;
            counter = 0;
            *output_redirect = REDIRECT_OUTPUT_ON;
            while (1){
                if (input_buffer[outer_counter] == ' ' || input_buffer[outer_counter] == NEWLINE ||
                    outer_counter == input_length) break;

                output_redirection_path[counter] = input_buffer[outer_counter];
                counter++;
                outer_counter++;
            }
            output_redirection_path[counter] = NULL_CHAR;
            strcpy(output_path, output_redirection_path);
        } else if (input_buffer[outer_counter] == '<' && outer_counter < (input_length - 1) &&
                   input_buffer[outer_counter + 1] == ' '){

            outer_counter++;
            outer_counter++;
            counter = 0;
            *input_redirect = REDIRECT_INPUT_ON;
            while (1){
                if (input_buffer[outer_counter] == ' ' || input_buffer[outer_counter] == NEWLINE ||
                    outer_counter == input_length) break;

                input_redirection_path[counter] = input_buffer[outer_counter];
                counter++;
                outer_counter++;
            }
            input_redirection_path[counter] = NULL_CHAR;
            strcpy(input_path, input_redirection_path);
        } else if (input_buffer[outer_counter] == '$' && (outer_counter < input_length - 1) &&
                   input_buffer[outer_counter + 1] == '$'){

            outer_counter++;

            for (counter = 0; counter < strlen(pid_str); counter++){
                command_string[inner_counter] = pid_str[counter];
                inner_counter++;
            }
        } else {
            command_string[inner_counter] = input_buffer[outer_counter];
            inner_counter++;
        }
    }

    counter = strlen(command_string);

    if ((counter > 1) && (command_string[counter - 1] == '&') && (command_string[counter - 2] == ' ')){

        command_string[counter - 1] = NULL_CHAR;    //remove the & character from the arg array
        command_string[counter - 2] = NULL_CHAR;    //remove the ' ' character from the arg array
        if (gbl_BG_MODE == BG_MODE_ON){
            *background_mode = BG_MODE_ON;
            *input_redirect = REDIRECT_INPUT_ON;
            *output_redirect = REDIRECT_OUTPUT_ON;
        }
    }


    for (word = strtok(command_string, CMD_DELIMITER); word; word = strtok(NULL, CMD_DELIMITER)){
        command_words[*command_counter] = word;
        *command_counter = *command_counter + 1;
    }

    return EXIT_SUCCESS;
}

void order_66(int death_note[]){
    for (size_t i = 0; i < CHILDREN_MAX; i++){
        if (death_note[i] > 0){
            // ask to die nicely
            kill(death_note[i], SIGTERM);
        }
    }
    for (size_t i = 0; i < CHILDREN_MAX; i++){
        if (death_note[i] > 0){
            // end of watch
            kill(death_note[i], SIGKILL);
        }
    }
}


/****************************************************************
 *                        local_functions
 *
 * This function uses the input commands from the user to decide
 * if the command is a local function or not. If it's one of the
 * defined functions it will call the local function and return
 * EXIT_SUCCESS. Otherwise, it will just return EXIT_SUCCESS. On
 * failure, EXIT_FAILURE is returned.
****************************************************************/


void local_functions(int argc, char **argv, int *function_type, int *fg_status,
                     int *process_type, int background_mode, int death_note[]){
    int exit_status = EXIT_SUCCESS;
    if (strcmp(argv[0], "cd") == 0){
        exit_status = local_cd(argc, argv);
        *fg_status = exit_status;
        *process_type = PARENT_PROCESS;
    } else if (strcmp(argv[0], "status") == 0){
        //do status here
        local_status(*fg_status);
        *process_type = PARENT_PROCESS;
    } else if (strcmp(argv[0], "exit") == 0){
        order_66(death_note);
        exit(EXIT_SUCCESS);
    } else if (strncmp(argv[0], "#", 1) == 0){
        *function_type = LOCAL_FUNCTION;
        *process_type = PARENT_PROCESS;
    } else {
        *function_type = EXEC_FUNCTION;
        if (background_mode == BG_MODE_ON && gbl_BG_MODE == BG_MODE_ON) {
            *process_type = BG_CHILD_PROCESS;
        } else {
            *process_type = FG_CHILD_PROCESS;
        }
    }
}

/****************************************************************
 *                        prepare_terminal
 *
 * This function prints the input character to the screen ':'
 * and then flushes stdout.
****************************************************************/

void prepare_terminal(){
    errno = 0;
    //TODO: ^ why is this here?
    fprintf(stdout, "%c", ':');
    fflush(stdout);
    if (errno != 0){
        fail("prepare_terminal", 1, 1);
    }
}

/****************************************************************
 *                        main
 *
 * This function uses a loop to emulate a terminal application.
 * It will collect input from a user and then process the inputs
 * just like a standard terminal.
****************************************************************/

int main(){

    char* cmd_argv[COMMAND_ARG_MAX];        //to hold the command "words" after tokenizing
    pid_t temp_pid;
    int cmd_argc;                       //to hold the quantity of commands after tokenizing
    int fg_status;                      //to hold the exit status of the last foreground process
    int bg_status; //to hold the last background status
    int process_type;                   //to hold the type of process, parent, bg child, fg child etc.
    int background_mode;
    int input_redirection;                   //for input redirection
    int output_redirection;                  //for output redirection
    char input_redirection_path[COMMAND_LEN_MAX];                //to hold the filename of the input redirection
    char output_redirection_path[COMMAND_LEN_MAX];               //to hold the filename of the output redirection
    int function_type; //the type of function to be executed
    char *dev_null = "/dev/null";
    int death_note[CHILDREN_MAX];

    for (size_t i = 0; i < CHILDREN_MAX; i++){
        death_note[i] = 0;
    }

    while(1){

        temp_pid = waitpid(-1, &bg_status, WNOHANG);
        if (temp_pid > 0){
            child_handler(temp_pid, bg_status, BG_MODE_ON);
            for (size_t i = 0; i < CHILDREN_MAX; i++){
                if (death_note[i] == temp_pid){
                    death_note[i] = 0;
                }
            }
        }
        process_type = PARENT_PROCESS;
        function_type = LOCAL_FUNCTION;
        for (int i = 0; i < COMMAND_ARG_MAX; i++){
            cmd_argv[i] = 0;
        }
        //memset(cmd_argv, NULL_CHAR, COMMAND_ARG_MAX);
        memset(input_redirection_path, NULL_CHAR, COMMAND_LEN_MAX);
        memset(output_redirection_path, NULL_CHAR, COMMAND_LEN_MAX);
        strcpy(input_redirection_path, dev_null); //default to /dev/null if not specified
        strcpy(output_redirection_path, dev_null); //default to /dev/null if not specified
        cmd_argc = 0;
        sig_handlers(PARENT_PROCESS);
        background_mode = BG_MODE_OFF;
        input_redirection = REDIRECT_INPUT_OFF;
        output_redirection = REDIRECT_OUTPUT_OFF;

        while (cmd_argc == 0){
            if (temp_pid > 0){
                child_handler(temp_pid, bg_status, BG_MODE_ON);
                for (size_t i = 0; i < CHILDREN_MAX; i++){
                    if (death_note[i] == temp_pid){
                        death_note[i] = 0;
                    }
                }
            }
            prepare_terminal();
            get_input(cmd_argv, &cmd_argc, &background_mode,
                      input_redirection_path,output_redirection_path,
                      &input_redirection, &output_redirection);
        }

        local_functions(cmd_argc, cmd_argv, &function_type, &fg_status,
                        &process_type, background_mode, death_note);
        if (function_type == EXEC_FUNCTION){

            exec_me(cmd_argv, process_type, input_redirection_path, output_redirection_path, input_redirection,
                    output_redirection, &fg_status, death_note);

        }

        temp_pid = waitpid(-1, &bg_status, WNOHANG);
        if (temp_pid > 0){
            child_handler(temp_pid, bg_status, BG_MODE_ON);
            for (size_t i = 0; i < CHILDREN_MAX; i++){
                if (death_note[i] == temp_pid){
                    death_note[i] = 0;
                }
            }
        }
    }
}