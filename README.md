A micro-shell, ush (ie, μ-shell), modeled after csh.

To run this project:
- Clone repo, cd into the directory.
- from command line:
    ```
    $ make
    $ ./ush
    
    ```

### To run tests:
- dependency - csh
    + install csh : `sudo apt install csh`
- Run tests: `make test`
- For all successful tests- output will be something like this:
```
bash test.sh
running tests/test_builtins.txt
running tests/test_exec.txt
running tests/test_pipeserr.txt
running tests/test_pipes.txt
running tests/test_stderr.txt
running tests/test_stdinouterr.txt
running tests/test_stdin.txt
running tests/test_stdoutapp.txt
running tests/test_stdout.txt
```

### Extra
- add ush to path in bash
    `export PATH=/home/user/dir/:$PATH`

Description:
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
