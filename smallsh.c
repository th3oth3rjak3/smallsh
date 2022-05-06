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

void child_handler(pid_t pid, int* status, int bg_proc){
    if (bg_proc == BG_CHILD_PROCESS){
        char pid_str[PID_MAX_CHARS]={0};
        sprintf(pid_str, "%d", pid);
        if (WIFEXITED(*status)){
            fprintf(stdout, "PID %s finished with exit status: %d\n", pid_str, WEXITSTATUS(*status));
            fflush(stdout);
        } else if (WIFSIGNALED(*status)){
            fprintf(stdout, " PID %s terminated by signal: %d\n", pid_str, WTERMSIG(*status));
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
    // TODO: print a useful message, don't allow background commands anymore
    // until the user sends another SIGTSTP
    errno = 0;
    char msg[SMALL_BUFFER];
    memset(msg, '\0', SMALL_BUFFER);
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
    // TODO: help a child end itself when SIGINT is sent to foreground
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
        //signal(SIGCHLD, SIG_IGN); //TODO: SIGCHLD, do_something
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

int local_status(int* fg_status){
    if (WIFEXITED(*fg_status)){
        fprintf(stdout, "Exit status: %d\n", WEXITSTATUS(*fg_status));
        fflush(stdout);
    } else if (WIFSIGNALED(*fg_status)){
        fprintf(stdout, "Terminated by signal: %d\n", WTERMSIG(*fg_status));
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

int exec_me(char* argys[], int bg_proc, char* input, char* output, int in, int out, int* fg_status){

    int fd0 = -1;
    int fd1 = -1;

    char my_in[255];
    char my_out[255];
    memset(my_in, '\0', 255);
    memset(my_out, '\0', 255);

    if(bg_proc == BG_CHILD_PROCESS && gbl_BG_MODE == BG_MODE_ON){
        if(in == 0){
            strcpy(my_in, "/dev/null");
        } else {
            if (strncmp(input, "./", 2) != 0){
                strcpy(my_in, "./");
                strcat(my_in, input);
            } else {
                strcpy(my_in, input);
            }
        }
        if(out == 0){
            strcpy(my_out, "/dev/null");
        } else {
            if (strncmp(output, "./", 2) != 0){
                strcpy(my_out, "./");
                strcat(my_out, output);
            } else {
                strcpy(my_out, output);
            }
        }
    }

    temp_pid = fork();
    switch (temp_pid){
        case -1:
            //error
            fail("fork", 1, 1);
        case 0:
            /* Be childish */

            if (bg_proc == BG_CHILD_PROCESS && gbl_BG_MODE == BG_MODE_ON){
                sig_handlers(BG_CHILD_PROCESS);
            } else {
                sig_handlers(FG_CHILD_PROCESS);
            }

            if (in){
                fd0 = open(input, O_RDONLY);
                dup2(fd0, STDIN_FILENO); // Magic number 0 because STDIN_FILENO didn't work.
                close(fd0);
            }
            if (out){
                fd1 = open(output, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                dup2(fd1, STDOUT_FILENO); // Magic number 1 because STDIN_FILENO didn't work.
                close(fd1);
            }

            execvp(argys[0], argys);
            if (errno != 0) {
                //shouldn't get to the next line if things go to plan.
                char fail_msg[100];
                strcpy(fail_msg, argys[0]);
                errno = EINVAL;
                fail(fail_msg, 1, 1);
            }

        default:
            //parent
            sig_handlers(PARENT_PROCESS);
            if (bg_proc == PARENT_PROCESS || gbl_BG_MODE == BG_MODE_OFF){
                waitpid(temp_pid, fg_status, 0);
            } else {
                fprintf(stdout, "Background PID: %d\n", temp_pid);
                fflush(stdout);
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

int get_input(char* args[], int* argc, int* bg_proc, char* input_val, char* output_val, int* in_val, int* out_val){

    char buf[COMMAND_LEN_MAX + 1];
    char cmds[COMMAND_LEN_MAX + 1];
    fgets(buf, COMMAND_LEN_MAX, stdin);

    int i;
    int j = strlen(buf);
    int k = 0;
    size_t x;
    pid_t pid = getpid();
    char pid_str[PID_MAX_CHARS];
    sprintf(pid_str, "%d", pid);
    int out = 0;
    int in = 0;
    int bg = 0;
    char output[COMMAND_LEN_MAX] = {0};
    char input[COMMAND_LEN_MAX] = {0};
    char * word;


    for (i = 0; i < j; i++){
        if (buf[i] == '\n'){
            cmds[k] = '\0';
            k++;
        } else if (buf[i] == '>' && i < (j - 1) && buf[i + 1] == ' '){
            i++;
            i++;
            x = 0;
            out = 1;
            while (1){
                if (buf[i] == ' ' || buf[i] == '\n' || i == j) break;
                output[x] = buf[i];
                x++;
                i++;
            }
            output[x] = '\0';
        } else if (buf[i] == '<' && i < (j - 1) && buf[i + 1] == ' '){
            i++;
            i++;
            x = 0;
            in = 1;
            while (1){
                if (buf[i] == ' ' || buf[i] == '\n' || i == j) break;
                input[x] = buf[i];
                x++;
                i++;
            }
            input[x] = '\0';
        } else if (buf[i] == '$' && (i < j - 1) && buf[i + 1] == '$'){
            i++;
            //i++;
            for (x = 0; x < strlen(pid_str); x++){
                cmds[k] = pid_str[x];
                k++;
            }
        } else {
            cmds[k] = buf[i];
            k++;
            //i++;
        }
    }

    k = strlen(cmds);
    if ((k > 1) && (cmds[k - 1] == '&') && (cmds[k - 2] == ' ')){
        cmds[k - 1] = '\0';
        cmds[k - 2] = '\0';
        bg = BG_CHILD_PROCESS;
    }

    *in_val = in;
    if (errno != 0) fail("in_val", 1, 1);
    *out_val = out;
    if (errno != 0) fail("out_val", 1, 1);
    *bg_proc = bg;
    if (errno != 0) fail("bg_proc", 1, 1);
    strcpy(output_val, output);
    if (strcmp(output_val, output) != 0) fail("output_val", 1, 1);
    strcpy(input_val, input);
    if (strcmp(output_val, output) != 0) fail("input_val", 1, 1);

    int y = 0;
    errno = 0;
    for (word = strtok(cmds, CMD_DELIMITER); word; word = strtok(NULL, CMD_DELIMITER)){
        args[y] = word;
        y++;
        if (errno != 0){
            fail("strtok", 1, 1);
        }
    }
    *argc = y;

    return EXIT_SUCCESS;
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

//TODO: is EXIT_FAILURE actually ever returned in here?

int local_functions(int argc, char**argv, int* function_type, int* fg_status){
    int exit_status = EXIT_SUCCESS;
    if (strcmp(argv[0], "cd") == 0){
        exit_status = local_cd(argc, argv);
        *fg_status = exit_status;
    } else if (strcmp(argv[0], "status") == 0){
        //do status here
        exit_status = local_status(fg_status);
    } else if (strcmp(argv[0], "exit") == 0){
        //do exit here
        //TODO: implement cleanup measures on
        exit(EXIT_SUCCESS);
    } else if (strncmp(argv[0], "#", 1) == 0){
        *function_type = LOCAL_FUNCTION;
    } else {
        *function_type = EXEC_FUNCTION;
    }
    return exit_status;
}

/****************************************************************
 *                        prepare_terminal
 *
 * This function prints the input character to the screen ':'
 * and then flushes stdout.
****************************************************************/

void prepare_terminal(){
    //errno = 0;
    //TODO: ^ why is this here?
    fprintf(stdout, "%c", ':');
    fflush(stdout);
    if (errno != 0){
        fail("prepare_terminal", 1, 1);
    }
}

/****************************************************************
 *                     variable_cleanup
 *
 * This function is used to clean up previous variable values.
****************************************************************/

/*
void variable_cleanup(char **cmd_argv[], int *cmd_argc, int *fg_status,
int *bg_proc, int *last_bg_proc, ){

}
TODO: Finish this mess...
*/

/****************************************************************
 *                        main
 *
 * This function uses a loop to emulate a terminal application.
 * It will collect input from a user and then process the inputs
 * just like a standard terminal.
****************************************************************/

int main(){

    char* cmd_argv[COMMAND_ARG_MAX];        //to hold the command "words" after tokenizing
    memset(cmd_argv, '\0', COMMAND_ARG_MAX + 1);
    int cmd_argc = 0;                       //to hold the quantity of commands after tokenizing
    int fg_status = 0;                      //to hold the exit status of the last foreground process
    int bg_proc = PARENT_PROCESS;
    int in = REDIRECT_INPUT_OFF;                   //for input redirection
    int out = REDIRECT_OUTPUT_OFF;                  //for output redirection
    char input[COMMAND_LEN_MAX + 1];                //to hold the filename of the input redirection
    char output[COMMAND_LEN_MAX + 1];               //to hold the filename of the output redirection
    memset(input, '\0', COMMAND_LEN_MAX + 1);
    memset(output, '\0', COMMAND_LEN_MAX + 1);

    // Used to decide if exec function or local function.
    int function_type;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    while(1){
        bg_proc = PARENT_PROCESS;
        function_type = LOCAL_FUNCTION;
        memset(cmd_argv, '\0', COMMAND_ARG_MAX + 1);
        memset(input, '\0', COMMAND_LEN_MAX + 1);
        memset(output, '\0', COMMAND_LEN_MAX + 1);
        cmd_argc = 0;
        sig_handlers(PARENT_PROCESS);

        while (cmd_argc == 0){
            prepare_terminal();
            get_input(cmd_argv, &cmd_argc, &bg_proc, input, output, &in, &out);
        }

        local_functions(cmd_argc, cmd_argv, &function_type, &fg_status);
        if (function_type == EXEC_FUNCTION){

            exec_me(cmd_argv, bg_proc, input, output, in, out, &fg_status);

        }
    }
#pragma clang diagnostic pop
}
