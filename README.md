# smallsh

This is a simpel shell program developed for Unix-like systems, developed and tested in a CentOS 6 server.
- System calls are handled through the use of execvp() functions, which allows for simple use of the operating system's executables.
- Commands `exit`, `status`, and `cd` are self-developed and are independent of the rest of the system call operations.
- Forking and children processes are use for the purpose of executing commands. First a new child process will be "forked", in which the string arguments of the command will be processed for certain operations such as redirection (`>`) or process id (`$$`). After the string is processed and the arguments are in the desired state for execution, they are executed using `execvp()`. Should this execution fail, an error will be thrown.
- Foreground and background processes are supported using the `&` symbol at the end of a command. States of these child processes are checked with `waitpid()` with every loop.
- If desired, you can run another smallsh process within the main smallsh process!

# Usage
To compile in a Linux environment with gcc installed, run the command:
```
gcc -o smallsh smallsh.c
```

Then run:
```
./smallsh
```

If warnings pop up, they can be ignored in this current iteration of the program. 

To run in foreground-mode (that is to say, any usage of `&` in commands will be ignored and every command will be run in the foreground only), click the key combo CTRL-Z. The same key press is used to exit foreground-mode. To exit the shell, type `exit` into the shell prompt.
