##Ush micro shell
(ie, μ-shell), modeled after csh.

###Specification:
- [Manpage](ush_manpage.pdf), [Spec](spec.md)

### To run Ush:
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