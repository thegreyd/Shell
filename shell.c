#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

int pipefd[2];
int oldpipefd[2];
int status,i;
int fdin, fdout;
struct sigaction sa;

typedef struct {
  char *cmd;
  int (*handler)(char **);
} inbuilt_cmd;

int handle_unsetenv(char **);
int handle_setenv(char **);
int handle_getenv(char **);
int handle_exit(char **);
int handle_cd(char **);

inbuilt_cmd inbuilt_cmds[] = {
  {"getenv", handle_getenv},
  {"setenv", handle_setenv},
  {"unsetenv", handle_unsetenv},
  {"fg",     NULL},
  {"bg",     NULL},
  {"cd",     handle_cd},
  {"end",    handle_exit},
  {"exit",   handle_exit}
};

int handle_exit(char **args){
    exit(0);
}

int handle_cd(char **args){
    int status;
    //todo handle error when args doesn't have 1 and 2
    status = chdir(args[1]);
    if ( status < 0 ) {
        perror("cd");
        return -1;
    }
    return 0;
}

int handle_setenv(char **args)
{
    int status;
    //todo handle error when args doesn't have 1 and 2
    status = setenv(args[1],args[2],1);
    if ( status < 0 ) {
        perror("setenv");
        return -1;
    }
    return 0;
}

int handle_unsetenv(char **args)
{
    int status;
    //todo handle error when args doesn't have 1
    status = unsetenv(args[1]);
    if ( status < 0 ) {
        perror("unsetenv");
        return -1;
    }
    return 0;
}

int handle_getenv(char **args)
{   
    char *var;
    //todo handle error when args doesn't have 1
    var = getenv(args[1]);
    if ( var == NULL ) {
        fprintf(stderr,"cannot find env var %s\n", args[1]);
        return -1;
    }
    printf("%s\n",var);
    return 0;
}

static void execCmd(Cmd c)
{
    int i;
    pid_t childpid;
    if ( c ) {
        fprintf(stderr,"%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
        if ( c->in == Tin )
            fprintf(stderr,"<(%s) ", c->infile);
        if ( c->out != Tnil )
            switch ( c->out ) {
                case Tout:
                    fprintf(stderr,">(%s) ", c->outfile);
                    break;
                case Tapp:
                    fprintf(stderr,">>(%s) ", c->outfile);
                    break;
                case ToutErr:
                    fprintf(stderr,">&(%s) ", c->outfile);
                    break;
                case TappErr:
                    fprintf(stderr,">>&(%s) ", c->outfile);
                    break;
                case Tpipe:
                    fprintf(stderr,"| ");
                    break;
                case TpipeErr:
                    fprintf(stderr,"|& ");
                    break;
                default:
                    fprintf(stderr, "Shouldn't get here\n");
                    exit(-1);
            }

        if ( c->nargs > 1 ) {
            fprintf(stderr,"[");
            for ( i = 1; c->args[i] != NULL; i++ )
                fprintf(stderr,"%d:%s,", i, c->args[i]);
            fprintf(stderr,"\b]");
        }
        putchar('\n');
        // this driver understands one command

        //check for builtin commands
        for(i = 0; i < sizeof(inbuilt_cmds)/sizeof(*inbuilt_cmds); i++) {
            if( strcmp(c->args[0],inbuilt_cmds[i].cmd)==0 ) {
                if( inbuilt_cmds[i].handler != NULL ) {
                    inbuilt_cmds[i].handler(c->args);
                }
                return;
            }
        }
                
        //handle pipe
        if ( c->out == Tpipe || c->out == TpipeErr ){ 
        	pipe(pipefd);
        }

        childpid = fork();
        switch ( childpid ) {
            case 0:
            	// handle signals
                // set handler to default behaviour
                sa.sa_handler = SIG_DFL;
                //handle sigint cntl+c child doesn't ignore
                status = sigaction(SIGINT, &sa, NULL);
                if ( status == -1 ) {
                    perror("Error: cannot handle SIGINT");
                }
                //handle sigquit cntl+"\" child doesn't ignore
                status = sigaction(SIGQUIT, &sa, NULL);
                if ( status == -1 ) {
                    perror("Error: cannot handle SIGQUIT");
                }
                
                //deal with input redirect
                if ( c->in == Tin ) {
                    fdin = open(c->infile,O_RDONLY);
                    if ( fdin < 0 ) {
                        perror("open error");
                        exit(0);
                    }
                    
                    status = dup2(fdin, STDIN_FILENO);
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);   
                    }
                    close(fdin);
                }

                //deal with output redirect
                if ( c->out == Tout ) {
                    fdout = open(c->outfile, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                    if ( fdin < 0 ) {
                        perror("open error");
                        exit(0);
                    }
                    
                    status = dup2(fdout, STDOUT_FILENO);
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);   
                    }
                    close(fdout);
                }

                //deal with output append redirect
                if ( c->out == Tapp ) {
                    fdout = open(c->outfile, O_WRONLY|O_CREAT|O_APPEND, 0666);
                    if ( fdin < 0 ) {
                        perror("open error");
                        exit(0);
                    }
                    status = dup2(fdout, STDOUT_FILENO);
                    if ( status < 0 ) {
                        perror("dup2"); 
                        exit(0);   
                    }
                    close(fdout);
                }

                //deal with outputerror redirect
                if ( c->out == ToutErr ) {
                    fdout = open(c->outfile, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                    if ( fdin < 0 ) {
                        perror("open error");
                        exit(0);
                    }
                    status = dup2(fdout, STDOUT_FILENO);
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);   
                    }
                    status = dup2(fdout, STDERR_FILENO);
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);   
                    }
                    close(fdout);
                }

                //deal with output append error redirect
                if ( c->out == TappErr ) {
                    fdout = open(c->outfile, O_WRONLY|O_CREAT|O_APPEND, 0666);
                    if ( fdin < 0 ) {
                        perror("open error");
                        exit(0);
                    }
                    status = dup2(fdout, STDOUT_FILENO);
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);   
                    }
                    status = dup2(fdout, STDERR_FILENO);
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);   
                    }
                    close(fdout);
                }

                // deal with pipes and pipeErr redirects
                if ( c->in == Tpipe || c->in == TpipeErr ) {
                	status = dup2(oldpipefd[0], STDIN_FILENO);
                	if ( status < 0 ) {
                        perror("dup2");
                        exit(0);
                    }
                    close(oldpipefd[0]);
                }

                if ( c->out == Tpipe ) {
            		status = dup2(pipefd[1],  STDOUT_FILENO);	
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);
                    }
                    close(pipefd[1]);
                }

                if ( c->out == TpipeErr ) {
                    status = dup2(pipefd[1],  STDOUT_FILENO);
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);
                    }
                    status = dup2(pipefd[1],  STDERR_FILENO);      
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);
                    }
                    close(pipefd[1]);
                }

                if ( c->in == Tpipe || c->in == TpipeErr ) {
                	close(oldpipefd[1]);
                }

                if ( c->out == Tpipe || c->out == TpipeErr ) {
                	close(pipefd[0]);
                }
                
                // execute the command
                status = execvp(c->args[0], c->args);
                // error is executable/command not found 
                if ( errno == 2 ) 
                    fprintf(stderr, "command not found\n");
                // error is permission denied
                else if ( errno == 13 )
                    fprintf(stderr, "permission denied\n");
                else
                    fprintf(stderr, "%d: %s\n", errno,strerror(errno));
                exit(EXIT_FAILURE);
            case -1:
                fprintf(stderr, "Fork error\n");
                exit(EXIT_FAILURE);
            default:
                wait(NULL);
                // deal with pipes and pipeerr
                if ( c->in == Tpipe || c->in == TpipeErr) {
                	close(oldpipefd[0]);
            	}

            	if ( c->out == Tpipe || c->out == TpipeErr) {
                	oldpipefd[0] = pipefd[0];
		            oldpipefd[1] = pipefd[1];
		            close(pipefd[1]);
            	}
        }
    }
}

static void execPipe(Pipe p)
{
    int i = 0;
    Cmd c;

    if ( p == NULL )
        return;

    fprintf(stderr,"Begin pipe%s\n", p->type == Pout ? "" : " Error");
    for ( c = p->head; c != NULL; c = c->next ) {
        fprintf(stderr,"  Cmd #%d: ", ++i);
        execCmd(c);
    }
    fprintf(stderr,"End pipe\n");
    execPipe(p->next);
}

int main(int argc, char *argv[])
{
    Pipe p;
    char *host;

    sa.sa_handler = SIG_IGN;
    //handle sigint cntl+c parent ignores
    status = sigaction(SIGINT, &sa, NULL);
    if ( status == -1 ) {
        perror("Error: cannot handle SIGINT");
    }
    //handle sigquit cntl+"\" parent ignores
    status = sigaction(SIGQUIT, &sa, NULL);
    if ( status == -1 ) {
        perror("Error: cannot handle SIGQUIT");
    }

    host = getenv("USER");

    while ( 1 ) {
        fprintf(stderr,"%s%% ", host);
        p = parse();
        execPipe(p);
        freePipe(p);
    }
}

/*........................ end of main.c ....................................*/
