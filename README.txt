Author: Jake Hathaway
Date: 5/6/2022
Description: smallsh is a small shell application written in C.

To compile and use smallsh.c, enter the following into the terminal:

gcc -std=gnu99 -o smallsh smallsh.c -Wall -Wextra -Wpedantic -Werror

smallsh supports the following local commands:

cd - cd is used to change directories. If cd is typed with no arguments, it will change to the HOME directory as specified in the environment variable.
status - status is used to return the exit status of the last foreground process that was run. It works for local functions and exec functions.
exit - a function that cleans up any processes and closes itself.

smallsh also supports the exec() family of functions.

smallsh does not support spaces in pathnames.

smallsh supports input and output redirection using the following syntax. Example provided uses the ls function:

output redirection to myfile.txt
:ls > myfile.txt

input redirection from myfile.txt
:cat < myfile.txt

input and output redirection:
:cat < myfile.txt > mynewfile.txt

smallsh also supports variable expansion for $$ and converts it to the process ID of the terminal.
