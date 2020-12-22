# smallsh

This is a simple shell program that can run many simple commands by making use of the execvp() functions to make system calls as well as fork() to create children processes where these system calls run. It is also able to run background processes or if desired a foreground-only mode. To compile, simply run: 

gcc -o smallsh smallsh.c. 

Then to run:

./smallsh

The warnings can be ignored in this iteration of the program. To run in foreground-only mode, click CTRL-Z. The same command is used to exit foreground-mode. To exit the shell, type "exit" into the prompt. 