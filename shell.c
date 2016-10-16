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
int status;
int fdin, fdout;
struct sigaction sa;

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
        // exit if exit
        if ( !strcmp(c->args[0], "end") || !strcmp(c->args[0], "exit") )
           exit(0);
        
        if ( c->out == Tpipe ){ 
        	pipe(pipefd);
        }

        childpid = fork();
        switch ( childpid ) {
            case 0:
            	// handle signals
                sa.sa_handler = SIG_DFL;
                status = sigaction(SIGINT, &sa, NULL);

                if ( status == -1 ) {
                    perror("Error: cannot handle SIGINT");
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
                // deal with pipes
                if ( c->in == Tpipe ) {
                	close(oldpipefd[0]);
            	}

            	if ( c->out == Tpipe ) {
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
    status = sigaction(SIGINT, &sa, NULL);

    if ( status == -1 ) {
        perror("Error: cannot handle SIGINT");
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
