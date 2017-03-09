Implement the micro-shell, ush (ie, μ-shell).

Description:
Implement the Unix shell described in the accompanying man page. Do not to implement job control. Therefore, do not implement the following:

    the fg command,
    the bg command,
    the kill command,
    the jobs command, or
    the & operator. 

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
