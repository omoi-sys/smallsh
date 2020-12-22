/****************************************************************************************************
** Name: Abraham Serrato Meza
** E-mail: serratab@oregonstate.edu
** Program 3: smallsh
** This program is a shell with limited functionalities. It has three built-in functions:
** exit: exits the process to end program
** cd: moves working directory to specified directory, or in the case of none provided to HOME
** status: prints out the exit status of the last process that finished or was terminated by signal
** Additionally it makes use of fork() to create children processes that carry out system calls
** using execvp() after certain checks are made on the inputed arguments from the user. It also
** allows for a "foreground-only" mode in which background requests (those ending with &) are
** ignored and simply treated as foreground processes.
* ***************************************************************************************************
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

// bool switch button for foregrond/background mode
int ForegroundMode = 0; // 1 for true, 0 for false;

// list of background processes to check on
// before prompting again
int backP[1024];
int backPos = 0;

// current working directory
char working_dir[2048];

// signal termination handler using CTRL-C
void catchSIGINT(int signo) {
    // print out to stderr the term signal value
    fprintf(stdout, "terminated by signal %d", WTERMSIG(signo));
    fflush(stdout); // flush stdout
}

// stop process signal handle using CTRL-Z
void catchSIGTSTP(int signo){
    if(ForegroundMode == 0){
        ForegroundMode = 1; // update boolean to new state
        // message to print if we're going into foreground-only mode
        char* messageFMI = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, messageFMI, 50);
        fflush(stdout);
    } else if (ForegroundMode == 1) {
        ForegroundMode = 0; // uptdate boolean to new state
        // message to print if we're exiting the foreground-only mode
        char* messageFMO = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, messageFMO, 30);
        fflush(stdout);
    }
}

// status output
void statusOut(int childExitStatus);
// check to see if && is in an input string such as tempdir$$ and replace with pid
int checkCashCash(char* args[], int pid);
// carry out non-built in functions and foreground processes
void fork_function(char* args[], int* ampersand, int* exitStatus, int* childExitStatus);

int main() {
    int pid = getpid(); // process id
    char* command; // input
    size_t buffer = 0; // buffer for getline
    int running = 1; // boolean to run while loop
    int exitStatus = -5;
    int childExitStatus = -5;
    // initialize working directory to current directory
    strcpy(working_dir, getenv("PWD"));

    // Signal handling for CTRL-C
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Signal handling for CTRL-C
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);


    // run smallsh prompt and functionalities
    while(running) {
        // check background processes to see if any has finished and then print out any that has
        int bgi;
        for(bgi = 0; bgi < backPos; bgi++) {
            int child = -5;
			// if a background process has completed running print its pid
            if (backP[bgi] != 0 && waitpid(backP[bgi], &child, WNOHANG) != 0){
                fprintf(stdout, "background pid %d is done: ", backP[bgi]);
                fflush(stdout);
                statusOut(child);
				// remove process from list of background processes
                backP[bgi] = 0;
            }
        }


        int result = -5; // result of getline
        write(STDOUT_FILENO, ": ", 2);
        fflush(stdout);
		fflush(stdin);
        result = getline(&command, &buffer, stdin);
        if (result == -1)
            clearerr(stdin);
        // using read() causes a bug where children functionalities don't work
        // and breaks the built-in functions as well, look into if time allows
        //read(STDIN_FILENO, command, 2048);
        command[strlen(command)-1] = '\0'; // get rid of the new line

        // some funky stuff was experience when using the command string
        // after strtok so the input is copied into a temp string
        char temp[2048]; 
        strcpy(temp, command);
        char delim[] = " "; // delimeter to use on strtok
        char* token = strtok(temp, delim);

        // list of arguments
        char* args[512];
        int i = 0;

        int ampersand = 0;
        while(token) {
            // get the arguments into its own array
            // which will be important for use in
            // execvp() function in children
            args[i] = token;
            token = strtok(NULL, delim);

            // if & is found at the end of a command
            // and foreground mode is ON, remove &
            // as it's supposed to be ignored
            if (strcmp(args[i], "&") == 0){
                if (token == NULL && ForegroundMode == 0){
                    args[i] = NULL;
                    ampersand = 1;
                } else if (token == NULL && ForegroundMode == 1) {
                    args[i] = NULL;
                }
            }
            i++;
        }

        // Fill the remainder of the arguments with NULL.
        // Without this, an error will occur when using execvp
        // that will cause the program to do nothing and move
        // on to the next : prompt
        for (i; i < 512; i++) {
            args[i] = NULL;
        }

        // check for $$ in the code
        int found$$ = checkCashCash(args, pid);

        // ignore empty entries and comments
        if (command[0] == '#' || command[0] == '\0') {
            continue;
        }

        // built-in commands
        else if (strcmp(command, "exit") == 0) {
            running = 0; // end loop and go to end of program
        } else if (strcmp(args[0], "cd") == 0) {
            // if cd isn't called by itself, move to that directory
            if (args[1] != NULL) {
                // print error if the directory specified 
                // does not exist
                if(chdir(args[1]) == -1) {
                    char error_string[2048];
                    strcpy(error_string, args[1]);
                    strcat(error_string, ": no such file or directory\n");
					// use fprintf to avoid default error showing up
                    fprintf(stderr, error_string);
                    fflush(stderr);
                }
                else {
                    // get working directory
                    getcwd(working_dir, 2048);
                }
            } else {
                // if cd is called on its own,
                // change working directory to home directory
                chdir(getenv("HOME"));
                getcwd(working_dir, 2048);
            }
        } else if(strcmp(args[0], "status") == 0) {
            statusOut(childExitStatus);
        } else {
			// all non-built in operations run on this function as well
			// as the checks for input/output and foreground processes
            fork_function(args, &ampersand, &exitStatus, &childExitStatus); 
        }
    }

    return 0;
}


/****************************************************************************************************
* Function returns exit value or termination signal depending on the results of the input.
*****************************************************************************************************
*/
void statusOut(int childExitStatus) {
	// normal termination or error
    if (WIFEXITED(childExitStatus) != 0){
        fprintf(stdout, "exit value %d\n", WEXITSTATUS(childExitStatus));
        fflush(stdout);
    } else { // termination by signal
        fprintf(stdout, "terminated by signal %d\n", WTERMSIG(childExitStatus));
        fflush(stdout);
    }
}


/*****************************************************************************************************
* Function takes in the list of arguments and the process id of the current process. Then it 
* iterates over the array list checking for $$ in some argument, then replaces it with the
* process id and returns true as $$ was found in the function.
******************************************************************************************************
*/
int checkCashCash(char* args[], int pid) {
    int found = 0;
    int i;
    for(i = 0; args[i] != NULL; i++) {
        int j;
        for(j = 0; args[i][j] != '\0'; j++) {
			// if $$ is found in the argument, replace it
            if (args[i][j] == '$' && args[i][j+1] == '$') {
                char string_pid[32];
				// remove $$ from string
                args[i][j] = '\0';
                args[i][j + 1] = '\0';
				// convert process id into a string
                sprintf(string_pid, "%d", pid);
				// replace old $$ in argument with string process id
                strcat(args[i], string_pid);
                found = 1;
            }
            // end loop if $$ has been changed
            if (found == 1)
                break;
        }
        // end main loop if $$ has been changed
        if (found == 1)
            break;
    }

    return found;
}


/***************************************************************************************************
* Here is where the whole forking process takes place. The function takes the list of arguments,
* the value of ampersand to know if we need to run in foreground mode, the exit status to save
* the result of child, and the child exit status. When in the child the function will first 
* check for redirection needs, then open those files if needed and only after comfirming that
* those files are valid does it carry out the operation requested using execvp();
****************************************************************************************************
*/
void fork_function(char* args[], int* ampersand, int* exitStatus, int* childExitStatus) {
    int i;
    int spawnPid;
    spawnPid = fork();
    switch(spawnPid) {
        case -1:
			// something went wrong
            perror("Hull Breach!\n");
            fflush(stderr);
            exit(2);
            break;
        case 0:
            if (args[0]) {
                // check arguments for the redirect > character
                int redirect = 0; // boolean to see if we need to carry out a redirect
                int output_pos = -5; // position of output filename
                int input_pos = -5; // position of input filename
                for(i = 0; i < 512; i++) {
                    if (args[i] != NULL) {
						// > redirection found
                        if (strcmp(args[i], ">") == 0) {
                            redirect = 1;
                            output_pos = i + 1; // set position of output filename
                        } else if (strcmp(args[i], "<") == 0) { // < redirection found
                            redirect = 1;
                            input_pos = i + 1; // set position of input filename
                        } else if (args[i] == NULL) {
                            break;
                        }
                    }
                }

                // Handle redirections for both < and >
                if (redirect) {
                    int inputFD; // input file director
                    int outputFD; // output file director

                    // redirect stdin to input file IF it exists only,
                    // otherwise print an error and exit
                    if (input_pos != -5) {
                        // try to open input file
                        inputFD = open(args[input_pos], O_RDONLY);
                        if(inputFD == -1){
                            // custom error message for when file doesn't exist
                            char error_string[2048] = "cannot open ";
                            strcat(error_string, args[input_pos]);
                            strcat(error_string, " for input\n");
                            // we use fprintf to print to stderr due to the fact
                            // that using perror ends up also allowing a command
                            // such as wc to print its own error message as well
                            // which we want to avoid
                            fprintf(stderr, error_string);
                            fflush(stderr);
                            exit(1);
                        } else {
                            // redirect to stdin and close input file
                            dup2(inputFD, 0);
                            close(inputFD);
                        }
                    }
                    // redirect stdout to output file
                    if (output_pos != -5) {
                        // open and/or create/truncate output file and then close
                        // after stdout has been redirected to it
                        outputFD = open(args[output_pos], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        dup2(outputFD, 1);
                        close(outputFD);
                    }

                    // Null out everything from > and on
                    // as we must not have any other arguments
                    // for execvp after this.
                    if (output_pos != -5) {
                        for(i = output_pos - 1; i < 512; i++) {
                            args[i] = NULL;
                        }
                    }
                            

                    // Null out everything from < and on
                    // as we must not have any other arguments
                    // for execvp after this.
                    // This will not activate if we have no <
                    // redirection called
                    if (input_pos != -5) {
                        for (i = input_pos - 1; i < 512; i++) {
                            args[i] = NULL;
                        }
                    }
                }
                            
                // execute command
                // if executing the command does not return 0, print error
                if(execvp(args[0], args)) {
                    // same as above we use fprintf to do a custom error
                    // print to stderr
                    char error_string[2048];
                    strcpy(error_string, args[0]);
                    strcat(error_string, ": no such file or directory\n");
                    fprintf(stderr, error_string);
                    fflush(stderr);
                    exit(1);
                }
            }
            break;
        default:
            // if in foreground mode, move on to the next command even
			// if the child process isn't done
            if (*ampersand == 1) {
                if((waitpid(spawnPid, childExitStatus, WNOHANG)) == 0) {
					// print background pid
                    fprintf(stdout, "background pid is %d\n", spawnPid);
                    fflush(stdout);
                    backP[backPos] = spawnPid;
                    backPos++;
                }
            } else {
				// wait until child has terminated to give command back to the prompt
                *exitStatus = waitpid(spawnPid, childExitStatus, 0);
            }
            break;
    }
}