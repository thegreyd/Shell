## Description:

Implement the Unix shell described in the accompanying man page. 

    Summary of details enumerated in the manual page (in order of appearance):
        rc file (~/.ushrc)
        hostname%
        special chars:
        & | ; < > >> |& >& >>&
        backslash
        strings
        commands:
        cd echo logout nice pwd setenv unsetenv where 
    Major tasks
        Command line parsing
            ush.tar.gz 
        Command execution
            fork(2)
            exec family: execvp(2) might be the best 
        I/O redirection
            open(2), close(2) (not fopen, fclose)
            dup(2), dup2(3c)
            stdin, stdout, stderr 
        Environment variable
            putenv(3c), getenv(3c) 
        Signal handling
            signal(5)
            signal(3c)
            getpid(2), getpgrp(2), getppid(2), getpgid(2)
            setpgid(2), setpgrp(2) 

Job control is not implemented including these commands

    the fg command,
    the bg command,
    the kill command,
    the jobs command, or
    the & operator. 


# Behaviour:
	- disabled standard input buffer
    - Signal Handling
        + ignore sigint(ctrl+c) in parent shell
        + ignore sigquit(ctrl+\) in parent shell
        + ignore sigterm in parent shell
        + ignore sigtstp(ctrl+z) in parent shell
        + if cannot set sigaction perror "cannot handle signal"
    - .ushrc
        + search in home directory
        + no error if not found
        + open in read-only mode 
        + execute commands line by line
        + command behaviour is exactly like when normally run in shell
        + works by counting the number of lines in ushrc, redirecting the file to shells standard input and reading and executing line number of times.
        + restores the standard input to command line after execution
    - prompt "%hostname" - hostname is the environment variable "USER"
    - print prompt at standard output
    - print everything else at standard input

