/**************************************************************
C++ version of a small shell.
*/

#include <iostream>
#include <string>
#include <string.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

// switch button for foreground-only mode
bool foregroundMode = false;

// list of background processes to check on before prompting again
std::vector<int> backProcesses;
int backProcessPosition = 0;

std::string workingDir = "";

// signal termination handle using CTRL-C
void catchSIGINT(int signo) {
    // print out to stderr the term signal value and then flush
    std::cerr << "terminated by signal " << WTERMSIG(signo);
}

// stop process signal handle using CTRL-Z
void catchSIGTSTP(int signo) {
    if (!foregroundMode) {
        foregroundMode = true;
        std::cout << "\nEntering foreground-only mode (& is now ignored)\n" << std::flush;
    } else {
        foregroundMode = true;
        std::cout << "\nExiting foreground-only mode\n" << std::flush;
    }
}

void handleSigCTRLC() {
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);
}

void handleSigSTP() {
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}


void statusOutput(int childExitStatus);
// check to see if && is in an input string such as tempdir$$ and replace with pid
bool checkCashCash(std::vector<std::string> arguments, int pid);
// carry out non-built in functions and foreground processes
void fork_function(std::vector<std::string> arguments, bool *ampersand, int* exitStatus, int *childExitStatus);

int main() {
    int pid = getpid(); // process id
    std::string command; // input
    bool runLoop = true;
    int exitStatus = -5;
    int childExitStatus = -5;
    // initialize working directory to current directory
    workingDir = getenv("PWD");

    // Signal handling for CTRL-C
    handleSigCTRLC();
    handleSigSTP();

    // run smallsh prompt and functionalities
    while(runLoop) {
        // check background processes to see if any has finished and then print out any that has
        if (backProcesses.size() > 0) {
            for (auto process : backProcesses) {
                int child = -5;
                // if a background process has completed running, print its pid
                if (process != 0 && waitpid(process, &child, WNOHANG) != 0) {
                    std::cout << "background pid " << process << " is done: " << std::flush;
                    statusOutput(child);
                    process = 0;
                }
            }
        }

        std::cout << ": " << std::flush;
        std::getline(std::cin, command);

        std::vector<std::string> arguments;
        std::stringstream strstream(command);
        std::string current;
        bool ampersand = false;
        while(strstream >> current) {
            if (current == "&") {
                ampersand = true;
                continue;
            } else {
                arguments.push_back(current);
            }
        }

        // check for $$ in the code
        bool found$$ = checkCashCash(arguments, pid);

        // ignore empty entries and comments
        if (command[0] == '#' || command[0] == '\0') {
            continue;
        }

        // built-in commands
        else if (command == "exit") {
            runLoop = false; // end loop and go to end of program
            break;
        } else if (arguments[0] == "cd") {
            // if cd isn't called by itself, move to that directory
            if (arguments[1] != "") {
                if (chdir(arguments[1].c_str()) == -1) {
                    std::cerr << arguments[1] << ": no such file or directory\n" << std::flush;
                } else {
                    char tempDir[2048]; 
                    getcwd(tempDir, 2048); // get working directory
                    workingDir.assign(tempDir);
                }
            } else {
                // if cd is called on its own, change working directory to home directory
                chdir(getenv("HOME"));
                char tempDir[2048];
                getcwd(tempDir, 2048);
                workingDir.assign(tempDir);
            }
        } else if (arguments[0] == "status") {
            statusOutput(childExitStatus);
        } else {
            // all non-built in operations run on this function as well
            // as the checks for input/output and foreground processes
            fork_function(arguments, &ampersand, &exitStatus, &childExitStatus);
        }
    }

    return 0;
}

/******************************************************************************************
 * Function returns exit value or termination signal depending on the results of the input
 * ****************************************************************************************
 * */
void statusOutput(int childExitStatus) {
    // normal termination or error
    if (WIFEXITED(childExitStatus) != 0) {
        std::cout << "exit value " << WEXITSTATUS(childExitStatus) << std::endl << std::flush;
    } else {
        std::cout << "terminated by signal " << WTERMSIG(childExitStatus) << std::endl << std::flush;
    }
}

/********************************************************************************************
 * Function takes in the list of arguments and the process id for the current process.
 * */
bool checkCashCash(std::vector<std::string> arguments, int pid) {
    bool found = false;
    for(auto arg : arguments) {
        for(int j = 0; arg[j] != '\0'; j++) {
            // if $$ is found in the argument, replace it
            if (arg[j] == '$' && arg[j + 1] == '$') {
                // convert process id into string
                std::string pidString = std::to_string(pid);
                // remove $$ from string
                arg[j] = arg[j + 1] = '\0';
                // concatinate
                arg += pidString;
                found = true;
                break;
            }
        }

        if (found) {
            break;
        }
    }
    return found;
}

/*********************************************************************************************
 * Here is where the whole forking process takes place.
 * */
void fork_function(std::vector<std::string> arguments, bool *ampersand, int *exitStatus, int *childExitStatus) {
    int spawnPid = fork();
    switch(spawnPid) {
        case -1: // something went wrong
            std::cerr << "Hull Breach!\n" << std::flush;
            exit(2);
            break;
        case 0:
            if (arguments[0] != "") {
                // check arguments for the redirect > character
                bool redirect = false;
                int outputPosition = -5;
                int inputPosition = -5;
                for(int index = 0; index < arguments.size(); index++) {
                    if (arguments[index] != "") {
                        // > redirect found
                        if (arguments[index] == ">") {
                            redirect = true;
                            outputPosition = index + 1; // set position of output filename
                        } else if (arguments[index] == "<") { // redirect < found
                            redirect = true;
                            inputPosition = index + 1; // set position of input filename
                        } else if (arguments[index] != "") {
                            break;
                        }
                    }
                }

                // Handle redirections for both < and >
                if (redirect) {
                    std::ifstream inputFile;
                    std::ofstream outputFile;

                    // redirect stdin to input fiel IF it exists only,
                    // otherwise print an error and exit
                    if (inputPosition != -5) {
                        // try to open input file
                        inputFile.open(arguments[inputPosition]);
                        if (!inputFile.is_open()) {
                            std::cerr << "cannot open " << arguments[inputPosition] << " for input\n" << std::flush;
                            exit(1);
                        } else {
                            std::streambuf* stream_buffer_cin = std::cin.rdbuf();
                            // redirect to stdin and close input file
                            std::streambuf* stream_buffer_inputFile = inputFile.rdbuf();
                            std::cin.rdbuf(stream_buffer_inputFile);
                            inputFile.close();
                            //std::cin.rdbuf(stream_buffer_cin);
                        }
                    }

                    // redirect stdout to output file
                    if (outputPosition != -5) {
                        outputFile.open(arguments[outputPosition]);
                        std::streambuf* stream_buffer_cout = std::cout.rdbuf();
                        std::streambuf* stream_buffer_outputFile = outputFile.rdbuf();
                        std::cout.rdbuf(stream_buffer_outputFile);
                        outputFile.close();
                        //std::cout.rdbuf(stream_buffer_cout);
                    }

                    // Null out everyfrom from > and on
                    // as we must not have nay other arguments
                    // for execvp after this.
                    if (outputPosition != -5) {
                        arguments.erase(arguments.begin() + outputPosition, arguments.end());
                    }

                    // Null out everything from < and on
                    if (inputPosition != -5) {
                        arguments.erase(arguments.begin() + inputPosition, arguments.end());
                    }
                }

                // convert arguments to c string array since std::string vector isn't acceptable
                char *argCStrings[arguments.size() + 1];
                for(int index = 0; index < arguments.size(); index++) {
                    argCStrings[index] = const_cast<char*>(arguments[index].c_str());
                }
                argCStrings[arguments.size()] = NULL;
                // execute command
                // if executing the command does not return 0, print error
                if (execvp(arguments[0].c_str(), argCStrings)) {
                    std::cerr << arguments[0] << ": no such file or directory\n" << std::flush;
                    exit(1);
                }
                break;
            }
        default:
            // if in foreground mode, move on to the next command even
            // if the child process isn't done
            if (*ampersand) {
                if ((waitpid(spawnPid, childExitStatus, WNOHANG)) == 0) {
                    std::cout << "background pid is " << spawnPid << std::endl << std::flush;
                    backProcesses.push_back(spawnPid);
                    backProcessPosition++;
                }
            } else {
                // wait until child has terminated to give command back to the prompt
                *exitStatus = waitpid(spawnPid, childExitStatus, 0);
            }
            break;
    }
}