/****************************************************************
 * Name: Jake Hathaway
 * Date: 5/7/2022
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
#define NO_EXIT 1
#define DO_EXIT 0

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
****************************************************************/
int gbl_BG_MODE = BG_MODE_ON;
int gbl_EXIT = NO_EXIT;

/****************************************************************
 *                        bg_child_status
 *
 * This is a function that is called whenever waitpid returns a
 * child pid for a background child. It prints messages to the
 * screen.
****************************************************************/

void bg_child_status(pid_t pid, int status){

    if (WIFEXITED(status)){
        fprintf(stdout, "PID %d finished with exit status: %d\n", pid, WEXITSTATUS(status)); // If exited, exit status
        fflush(stdout);
    } else if (WIFSIGNALED(status)){
        fprintf(stdout, "PID %d terminated by signal: %d\n", pid, WTERMSIG(status));         // If signal, signal status
        fflush(stdout);
    }
}

/****************************************************************
 *                        parent_SIGTSTP
 * This signal handler catches the SIGTSTP signal and prints a
 * message to STDOUT.
****************************************************************/

void parent_SIGTSTP(){

    errno = 0;                                          // Clear the associated error with the signal
    char * outmsg;                                      // Initialize new message pointer
    int len = 0;                                        // Initialize a new length variable for the write command.
    if (gbl_BG_MODE == BG_MODE_ON){                     // If background commands are allowed...
        outmsg = "\nBackground Mode Disabled\n:";
        len = 27;
        gbl_BG_MODE = BG_MODE_OFF;                      // Set background mode to OFF
    } else if (gbl_BG_MODE == BG_MODE_OFF){             // When background commands are not allowed...
        outmsg = "\nBackground Mode Enabled\n:";
        len = 26;
        gbl_BG_MODE = BG_MODE_ON;                       // Set background mode to ON
    }
    write(STDOUT_FILENO, outmsg, len);                  // Write a nice reentrant safe message to the user
}

/****************************************************************
 *                        fg_child_SIGINT
 * This signal handler catches the SIGINT signal for any
 * foreground children and helps them terminate themselves.
****************************************************************/

void fg_child_SIGINT(){
    exit(1);    // The child will exit.
}

/****************************************************************
 *                   initialize_sig_handlers
 * This function defines and initializes the signal handlers
 * needed for each process type at various stages during the
 * program.
****************************************************************/

void sig_handlers(int proc_type){
    if (proc_type == PARENT_PROCESS){       // Parent process
        signal(SIGINT, SIG_IGN);            // Ignore SIGINT per the specs
        signal(SIGTSTP, parent_SIGTSTP);    // Handle SIGTSTP signals via parent_SIGTSTP handler
    }
    if (proc_type == FG_CHILD_PROCESS){     // Foreground child process
        signal(SIGINT, fg_child_SIGINT);    // Handle self-exit for FG child processes per the spec.
        signal(SIGTSTP, SIG_IGN);           // Ignore SIGTSTP signals.
    }
    if (proc_type == BG_CHILD_PROCESS){     // Background child process
        signal(SIGINT, SIG_IGN);            // Ignore SIGINT signals per the specs
        signal(SIGTSTP, SIG_IGN);           // Ignore SIGTSTP signals per the specs
    }
}

/****************************************************************
 *                        local_cd
 *
 * This function is a local implementation of the cd function to
 * change the current working directory. It takes zero or one
 * argument(s). If no argument is provided, then it will cd to
 * the HOME directory. Otherwise it will change the directory to
 * the path provided if it's valid. Returns 0 for success, 1 for
 * failure.
****************************************************************/

int local_cd(int argc, char** argv){

    char msg_arr[MAX_PATH];                     // This character array is used to build the error messages.
    memset(msg_arr, NULL_CHAR, MAX_PATH);       // Initialize array to null characters each time it's used.

    if (argc == 2){                             // When a path is supplied to the cd command
        if (chdir(argv[1]) == -1){              // If an error is detected, print a message
            strcpy(msg_arr, "cd: ");
            strcat(msg_arr, argv[1]);           // Show the user the argument that was incorrect, e.g. bad path
            strcat(msg_arr, ": ");
            strcat(msg_arr, strerror(ENOENT));  // Use the no such file or directory message.
            strcat(msg_arr, "\n");
            fprintf(stderr, "%s", msg_arr);
            fflush(stderr);                     // Flush stderr to prevent problems
            errno = 0;                          // Reset errno when it gets set during chdir
            return EXIT_FAILURE;
        }
    } else if (argc == 1){                      // When no argument is supplied, should go to the HOME directory
        if (chdir(getenv("HOME")) == -1) {      // Go to environment var HOME, -1 is error.
            strcpy(msg_arr, "cd: ");
            strcat(msg_arr, getenv("HOME"));    // Show the user where cd tried to go before failing.
            strcat(msg_arr, ": ");
            strcat(msg_arr, strerror(ENOENT));  // Use the no such file or directory message.
            strcat(msg_arr, "\n");
            fprintf(stderr, "%s", msg_arr);
            fflush(stderr);                     // Flush stderr to prevent problems
            errno = 0;                          // Reset errno when it gets set during chdir
            return EXIT_FAILURE;
        }
    } else {                                    // Case when too many arguments are supplied.
        strcpy(msg_arr, "cd: ");
        strcat(msg_arr, argv[2]);               // Show the user the first argument that caused the problem.
        strcat(msg_arr, ": ");
        strcat(msg_arr, strerror(EINVAL));      // Use the Invalid Argument message.
        strcat(msg_arr, "\n");
        fprintf(stderr, "%s", msg_arr);
        fflush(stderr);                         // Flush stderr to prevent problems
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
        fprintf(stdout, "Exit status: %d\n", WEXITSTATUS(status));          // If it exited, return status
        fflush(stdout);
    } else if (WIFSIGNALED(status)){
        fprintf(stdout, "Terminated by signal: %d\n", WTERMSIG(status));    // If signaled, return signal
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

    char msg_arr[MAX_PATH];                             // This character array is used to build the error messages.
    memset(msg_arr, NULL_CHAR, MAX_PATH);               // Initialize array to null characters each time it's used.
    pid_t temp_pid;                                     // Used for the pid after the fork
    int fd0;                                            // File descriptor for input redirection
    int fd1;                                            // File descriptor for output redirection
    char *modified_input_path;                          // The path to follow for redirection of input
    char *modified_output_path;                         // The path to follow for redirection of output
    char input_path_array[MAX_PATH];                    // An array used to help modify the path
    char output_path_array[MAX_PATH];                   // An array used to help modify the path
    memset(input_path_array, NULL_CHAR, MAX_PATH);      // Set to null characters
    memset(output_path_array, NULL_CHAR, MAX_PATH);     // Set to null characters


    if(input_redirection == REDIRECT_INPUT_ON) {
        if (strncmp(input_redirection_path, "./", 2) != 0 && strncmp(input_redirection_path, "/", 1) != 0) {
            strcpy(input_path_array, "./");                     // Add a ./ to the start of any files missing ./ or /
            strcat(input_path_array, input_redirection_path);   // Add the user supplied path
            modified_input_path = input_path_array;             // Point to the new array.

            if ((fd0 = open(modified_input_path, O_RDONLY)) == -1){   // Try to open the redirect file, fail if not able.
                strcpy(msg_arr, "< : ");
                strcat(msg_arr, input_redirection_path);        // Show the user the bad path
                strcat(msg_arr, ": ");
                strcat(msg_arr, strerror(ENOENT));              // Use the no such file or directory message.
                strcat(msg_arr, "\n");
                fprintf(stderr, "%s", msg_arr);
                fflush(stderr);                                 // Flush stderr to prevent problems
                errno = 0;                                      // Reset errno when it gets set during open
            } else {
                if(close(fd0) == -1){                           // Try to close the file, if failure, send message
                    strcpy(msg_arr, "Error closing : ");
                    strcat(msg_arr, input_redirection_path);    // Show the user the bad path we couldn't close.
                    strcat(msg_arr, "\n");
                    fprintf(stderr, "%s", msg_arr);
                    fflush(stderr);
                    errno = 0;
                }
            }
        } else {
            modified_input_path = input_redirection_path;
        }
    }

    if(output_redirection == REDIRECT_OUTPUT_ON){
        if (strncmp(output_redirection_path, "./", 2) != 0 && strncmp(output_redirection_path, "/", 1) != 0) {
            strcpy(output_path_array, "./");                        // Add ./ to files that don't start with ./ or /
            strcat(output_path_array, output_redirection_path);
            modified_output_path = output_path_array;
        } else {
            modified_output_path = output_redirection_path;
        }
    }

    temp_pid = fork();                                      // Create a child process using fork. Save pid.
    switch (temp_pid){
        case -1:
            strcpy(msg_arr, "fork : ");
            strcat(msg_arr, strerror(ECHILD));              // Use the No child processes message.
            strcat(msg_arr, "\n");
            fprintf(stderr, "%s", msg_arr);
            fflush(stderr);                                 // Flush stderr to prevent problems
            errno = ECHILD;
            exit(EXIT_FAILURE);
        case 0:

            if (process_type == BG_CHILD_PROCESS){
                sig_handlers(BG_CHILD_PROCESS);             // Reset the signal handlers for BG children
            } else {
                sig_handlers(FG_CHILD_PROCESS);             // Reset the signal handlers for FG children
            }

            if (input_redirection == REDIRECT_INPUT_ON){            // Redirect file descriptors to input path
                fd0 = open(modified_input_path, O_RDONLY);          // Open only for reading.
                dup2(fd0, STDIN_FILENO);                            // Point STDIN_FILENO to the new file descriptor
                close(fd0);                                         // Close extra File descriptor
            }
            if (output_redirection == REDIRECT_OUTPUT_ON) {         // Redirect file descriptors to output path
                fd1 = open(modified_output_path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                dup2(fd1, STDOUT_FILENO);                           // Flags above set to allow file creation
                close(fd1);                                         // Close extra file descriptor after duplication
            }

            execvp(argys[0], argys);                                // Supply the exec function array to execvp for use
            fprintf(stderr, "exec: %s\n", strerror(EINVAL));
            fflush(stderr);                                         // Flush stderr to prevent problems
            errno = ENOEXEC;
            exit(EXIT_FAILURE);                                     // Terminate the program if exec doesn't work.

        default:
            sig_handlers(PARENT_PROCESS);
            if (process_type == FG_CHILD_PROCESS || gbl_BG_MODE == BG_MODE_OFF){
                waitpid(temp_pid, fg_status, 0);                    // Wait for the child process if in the foreground or bg mode disabled.
                int temp = *fg_status;                              // Get the status of the finished child
                if (WIFEXITED(temp)){                               // Check for exit condition
                    if (WEXITSTATUS(temp) != 0){
                        //memset(msg_arr, NULL_CHAR, MAX_PATH);
                        fprintf(stderr, "Exited with status: %d\n", WEXITSTATUS(temp));
                        fflush(stderr);                             // Flush stderr to prevent problems
                        errno = 0;
                    }
                } else if (WIFSIGNALED(temp)){
                    fprintf(stderr, "Terminated by signal: %d\n", WTERMSIG(temp));
                    fflush(stderr);
                    errno = 0;
                }

            } else {
                fprintf(stdout, "Background PID: %d\n", temp_pid);  // Report background PID of BG child process
                fflush(stdout);                                     // Flush stdout to prevent problems
                for (int i = 0; i < CHILDREN_MAX; i++){
                    if (death_note[i] == 0){                        // Search through the array for an open space
                        death_note[i] = temp_pid;                   // Add child to array for use upon exit
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
 * input so it can determine if the process should run in the
 * background.
****************************************************************/

int get_input(char *command_words[], int *command_counter, int *background_mode, char *input_path,
              char *output_path, int *input_redirect, int *output_redirect){

    char *word = NULL;
    char input_buffer[COMMAND_LEN_MAX];                         // Stores the input from STDIN
    memset(input_buffer, NULL_CHAR, COMMAND_LEN_MAX);           // Set all values to null character
    char command_string[COMMAND_LEN_MAX];                       // Holds the formatted command string before tokenizing
    memset(command_string, NULL_CHAR, COMMAND_LEN_MAX);         // Set all values to null character
    fgets(input_buffer, COMMAND_LEN_MAX, stdin);                // Retrieve from STDIN
    size_t outer_counter;                                       // Outer loop
    size_t input_length = strlen(input_buffer);                 // Length of the user input (for looping)
    size_t inner_counter;                                       // Inner loop
    size_t counter;                                             // Generic multipurpose counter
    pid_t pid = getpid();                                       // Current process PID for $$ expansion
    char pid_str[PID_MAX_CHARS];                                // Converted to a string to store in command_string
    sprintf(pid_str, "%d", pid);                                // Convert PID to string
    char output_redirection_path[MAX_PATH] = {0};               // Stores the output redirection path
    char input_redirection_path[MAX_PATH] = {0};                // Stores the input redirection path
    memset(output_redirection_path, NULL_CHAR, MAX_PATH);       // Set to null characters
    memset(input_redirection_path, NULL_CHAR, MAX_PATH);        // Set to null characters

    *background_mode = BG_MODE_OFF;                             // Used to denote a foreground process
    *input_redirect = REDIRECT_INPUT_OFF;                       // Start with input redirection turned off
    *output_redirect = REDIRECT_OUTPUT_OFF;                     // Start with output redirection turned off
    *command_counter = 0;                                       // Count of the total command words
    inner_counter = 0;                                          // Initialize to 0

    strcpy(input_redirection_path, input_path);                 // Set to /dev/null in main before entry. Copy.
    strcpy(output_redirection_path, output_path);               // Set to /dev/null in main before entry. Copy.

    for (outer_counter = 0; outer_counter < input_length; outer_counter++){
        if (input_buffer[outer_counter] == NEWLINE){            // Remove the trailing newline if exists.
            command_string[inner_counter] = NULL_CHAR;
            inner_counter++;
        } else if (input_buffer[outer_counter] == '>' && outer_counter < (input_length - 1) &&
                   input_buffer[outer_counter + 1] == ' '){     // Checking for output redirection

            outer_counter++;
            outer_counter++;
            counter = 0;
            *output_redirect = REDIRECT_OUTPUT_ON;              // Turn output resdirection on
            while (1){
                if (input_buffer[outer_counter] == ' ' || input_buffer[outer_counter] == NEWLINE ||
                    outer_counter == input_length) break;       // Exit the endless loop

                output_redirection_path[counter] = input_buffer[outer_counter]; // Copy the output path 1 character at a time
                counter++;
                outer_counter++;
            }
            output_redirection_path[counter] = NULL_CHAR;       // Add a trailing null terminator
            strcpy(output_path, output_redirection_path);       // Copy back to original location
        } else if (input_buffer[outer_counter] == '<' && outer_counter < (input_length - 1) &&
                   input_buffer[outer_counter + 1] == ' '){     // Check for input redirection

            outer_counter++;
            outer_counter++;
            counter = 0;
            *input_redirect = REDIRECT_INPUT_ON;                // Turn input redirection on.
            while (1){
                if (input_buffer[outer_counter] == ' ' || input_buffer[outer_counter] == NEWLINE ||
                    outer_counter == input_length) break;       // Exit at a space or end of the command string

                input_redirection_path[counter] = input_buffer[outer_counter]; // Copy the input path 1 character at a time
                counter++;
                outer_counter++;
            }
            input_redirection_path[counter] = NULL_CHAR;        // Add a trailing null character
            strcpy(input_path, input_redirection_path);         // Copy to the original location
        } else if (input_buffer[outer_counter] == '$' && (outer_counter < input_length - 1) &&
                   input_buffer[outer_counter + 1] == '$'){     // Search for variable expansion

            outer_counter++;

            for (counter = 0; counter < strlen(pid_str); counter++){
                command_string[inner_counter] = pid_str[counter];   // Add the PID string to the command string
                inner_counter++;
            }
        } else {
            command_string[inner_counter] = input_buffer[outer_counter]; // Add any normal characters to the command string
            inner_counter++;
        }
    }

    counter = strlen(command_string);                           // Set counter to length of the string for use with &

    if ((counter > 1) && (command_string[counter - 1] == '&') && (command_string[counter - 2] == ' ')){

        command_string[counter - 1] = NULL_CHAR;                // Remove the & character from the arg array
        command_string[counter - 2] = NULL_CHAR;                // Remove the ' ' character from the arg array
        if (gbl_BG_MODE == BG_MODE_ON){                         // Check to see if BG mode is still enabled (Control-C)
            *background_mode = BG_MODE_ON;                      // Set BG mode to on
            *input_redirect = REDIRECT_INPUT_ON;                // Turn on input redirect /dev/null if not specified
            *output_redirect = REDIRECT_OUTPUT_ON;              // Turn on output redirect /dev/null if not specified
        }
    }
    command_string[counter] = NULL_CHAR;
    *command_counter = 0;
    for (word = strtok(command_string, CMD_DELIMITER); word; word = strtok(NULL, CMD_DELIMITER)){
        command_words[*command_counter] = malloc(sizeof(char) * COMMAND_LEN_MAX);
        sprintf(command_words[*command_counter], "%s", word);   // Store the word in the array;
        *command_counter = *command_counter + 1;                // Increment command_count for each word added.
    }

    return EXIT_SUCCESS;
}

/****************************************************************
 *                        order_66
 *
 * Order 66, also known as Clone Protocol 66, was a top-secret
 * order identifying all Jedi as traitors to the Galactic
 * Republic and, therefore, subject to summary execution by the
 * Grand Army of the Republic. The order was programmed into the
 * Grand Army clone troopers through behavioral modification
 * biochips implanted in their brains, making it almost
 * impossible for the clones to disobey the command to turn
 * against their Jedi Generals. The Kaminoan scientists who
 * designed the clone troopers believed it was only to be used
 * as a contingency protocol against renegade Jedi. In secret,
 * Order 66 was the means by which the Sith intended to bring
 * about the long-awaited fall of the Jedi Order.
 * https://starwars.fandom.com/wiki/Order_66
 *
 * In this program, order_66 terminates all the background
 * child processes when the main process is called upon to exit.
****************************************************************/

void order_66(int death_note[]){
    for (size_t i = 0; i < CHILDREN_MAX; i++){
        if (death_note[i] > 0){                     // PID's stored in death_note are greater than 0. Default value 0.
            kill(death_note[i], SIGTERM);           // Issue a polite SIGTERM kill command for gentle death.
        }
    }
    for (size_t i = 0; i < CHILDREN_MAX; i++){
        if (death_note[i] > 0){
            kill(death_note[i], SIGKILL);           // Issue a more demanding SIGKILL command for instant death.
        }
    }
}

/****************************************************************
 *                        local_functions
 *
 * This function uses the input commands from the user to decide
 * if the command is a local function or an exec command. It
 * also reads to see if the command is a comment.
****************************************************************/

void local_functions(int argc, char **argv, int *function_type, int *fg_status,
                     int *process_type, int background_mode, int death_note[]){
    int exit_status = EXIT_SUCCESS;                 // Initialize to 0
    if (strcmp(argv[0], "cd") == 0){
        exit_status = local_cd(argc, argv);         // Execute cd command and return its exit value
        *fg_status = exit_status;                   // Save the status to the status variable (used by status command)
        *process_type = PARENT_PROCESS;             // Keep the process type set to parent (don't execute a fork)
    } else if (strcmp(argv[0], "status") == 0){
        local_status(*fg_status);                   // Call local status to print the status message to the screen
        *process_type = PARENT_PROCESS;             // Keep the process type set to parent (don't execute a fork)
    } else if (strcmp(argv[0], "exit") == 0){
        for (int i = 0; i < COMMAND_ARG_MAX; i++){
            free(argv[i]);
        }
        order_66(death_note);                       // Issue order_66 kill command and exit
        gbl_EXIT = DO_EXIT;
    } else if (strncmp(argv[0], "#", 1) == 0){
        *function_type = LOCAL_FUNCTION;            // Set to local so exec logic in main will ignore
        *process_type = PARENT_PROCESS;             // No need to fork
    } else {
        *function_type = EXEC_FUNCTION;             // Any other command will be treated like an exec command
        if (background_mode == BG_MODE_ON && gbl_BG_MODE == BG_MODE_ON) {
            *process_type = BG_CHILD_PROCESS;       // If background mode is allowed and the user specified &
        } else {
            *process_type = FG_CHILD_PROCESS;       // Otherwise, create a fg child process.
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
    char msg_arr[SMALL_BUFFER];                         // Used for any error messages.
    memset(msg_arr, NULL_CHAR, SMALL_BUFFER);           // Initialize to null values
    errno = 0;                                          // Clear status before starting terminal
    fprintf(stdout, "%c", ':');
    fflush(stdout);
    if (errno != 0){                                    // Check for errors.
        strcpy(msg_arr, "Terminal: ");
        strcat(msg_arr, strerror(ENOTRECOVERABLE));     // Use the State not recoverable message
        strcat(msg_arr, "\n");
        fprintf(stderr, "%s", msg_arr);
        fflush(stderr);                                 // Flush stderr to prevent problems
        exit(EXIT_FAILURE);
    }
}

/****************************************************************
 *                        main
 *
 * Main houses the main logic for the looping process. It also
 * supplies many output variables for use throughout the smallsh
 * program.
****************************************************************/

int main(){

    char *cmd_argv[COMMAND_ARG_MAX];                            // To hold the command "words" after tokenizing
    pid_t temp_pid;                                             // To use with waitpid to store the returned value
    int cmd_argc;                                               // To hold the quantity of commands after tokenizing
    int fg_status;                                              // To hold the exit status of the last foreground process
    int bg_status;                                              // To hold the last background status
    int process_type;                                           // To hold the type of process, parent, bg child, fg child etc.
    int background_mode;                                        // Whether a process should run in the background/foreground
    int input_redirection;                                      // Indicates input redirection
    int output_redirection;                                     // Indicates output redirection
    char input_redirection_path[COMMAND_LEN_MAX];               // To hold the filename of the input redirection
    char output_redirection_path[COMMAND_LEN_MAX];              // To hold the filename of the output redirection
    int function_type;                                          // Local or exec function - type to be executed
    char *dev_null = "/dev/null";
    int death_note[CHILDREN_MAX];                               // To hold background child processes for termination upon parent exit

    for (size_t i = 0; i < CHILDREN_MAX; i++){
        death_note[i] = 0;                                      // Initialize to 0 values
    }

    while(gbl_EXIT == NO_EXIT){
        if (gbl_EXIT == NO_EXIT) {
            for (int i = 0; i < COMMAND_ARG_MAX; i++) {
                cmd_argv[i] = 0;
            }
            temp_pid = waitpid(-1, &bg_status, WNOHANG);            // Check for BG children finished
            if (temp_pid > 0) {
                bg_child_status(temp_pid, bg_status);
                for (size_t i = 0; i < CHILDREN_MAX; i++) {
                    if (death_note[i] == temp_pid) {
                        death_note[i] = 0;                          // If the child is found on the list, remove because it's done
                    }
                }
            }
            process_type = PARENT_PROCESS;                          // Start shell in foreground local mode
            function_type = LOCAL_FUNCTION;                         // Default to local function unless specified otherwise
            for (int i = 0; i < COMMAND_ARG_MAX; i++) {
                cmd_argv[i] = NULL;
            }
            memset(input_redirection_path, NULL_CHAR,
                   COMMAND_LEN_MAX);     // Clear array each time, set to null characters
            memset(output_redirection_path, NULL_CHAR,
                   COMMAND_LEN_MAX);    // Clear array each time, set to null characters
            strcpy(input_redirection_path, dev_null);                       // Default to /dev/null if not specified
            strcpy(output_redirection_path, dev_null);                      // Default to /dev/null if not specified
            cmd_argc = 0;                                                   // Set the command count to 0
            sig_handlers(
                    PARENT_PROCESS);                                   // Initialize the signal handlers as the parent
            background_mode = BG_MODE_OFF;                                  // Initialize background mode off to run in foreground
            input_redirection = REDIRECT_INPUT_OFF;                         // Turn off input redirection
            output_redirection = REDIRECT_OUTPUT_OFF;                       // Turn off output redirection

            while (cmd_argc == 0) {                                          // If user enters no data, loop until they do
                if (temp_pid > 0) {
                    bg_child_status(temp_pid, bg_status);                   // Give status on bg children finished
                    for (size_t i = 0; i < CHILDREN_MAX; i++) {
                        if (death_note[i] == temp_pid) {
                            death_note[i] = 0;                              // Remove finished children from death note list
                        }
                    }
                }
                prepare_terminal();                                         // Establish terminal character :
                get_input(cmd_argv, &cmd_argc, &background_mode,
                          input_redirection_path, output_redirection_path,
                          &input_redirection, &output_redirection);

                // Get input retrieves all the input
            }

            local_functions(cmd_argc, cmd_argv, &function_type, &fg_status,
                            &process_type, background_mode, death_note);

            // Local functions parses the input to determine if a local function or not.

            if (function_type == EXEC_FUNCTION) {

                exec_me(cmd_argv, process_type, input_redirection_path, output_redirection_path, input_redirection,
                        output_redirection, &fg_status, death_note);

                // exec_me is the function that runs an exec function and forks off into child processes

            }

            temp_pid = waitpid(-1, &bg_status,
                               WNOHANG);        // Check again for more children that have finished in the bg
            if (temp_pid > 0) {
                bg_child_status(temp_pid, bg_status);
                for (size_t i = 0; i < CHILDREN_MAX; i++) {
                    if (death_note[i] == temp_pid) {
                        death_note[i] = 0;
                    }
                }
            }
            for (int i = 0; i < COMMAND_ARG_MAX; i++){
                free(cmd_argv[i]);
            }
        }
        for (int i = 0; i < COMMAND_ARG_MAX; i++){
            free(cmd_argv[i]);
        }
    }
}