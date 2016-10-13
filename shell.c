#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

int pipefd[2];
int oldpipefd[2];
int status;
        
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
        	//if ( pipe_in_progress == 0 ){
        		pipe(pipefd);
        		fprintf(stderr,"pipe[%d,%d] created\n",pipefd[0],pipefd[1]);
        	//}
        	//else {
        	//	fprintf(stderr,"more pipe created\n");
        	//	pipe(morepipefd);	
        	//}
        	
        }

        //ls | grep h
        childpid = fork();
        switch ( childpid ) {
            case 0:
            	if ( c->in == Tpipe ) {
                	
                	//fprintf(stderr,"reading from %d\n",oldpipefd[0]);
                	//fprintf(stderr,"--inchild closed %d\n", oldpipefd[0]);
                	//fprintf(stderr,"--inchild closed %d\n", oldpipefd[1]);
                	status = dup2(oldpipefd[0], STDIN_FILENO);
                	if ( status<0 ){
            			perror("dup2 c->in");
            			exit(0);
                	}
                	close(oldpipefd[0]);
                	
				}

                if ( c->out == Tpipe ) {
            		//close(pipefd[0]);
            		//fprintf(stderr,"writing to %d\n",pipefd[1]);
            		//fprintf(stderr,"--inchild closed %d\n", pipefd[0]);
                	//fprintf(stderr,"--inchild closed %d\n", pipefd[1]);
            		status = dup2(pipefd[1],  STDOUT_FILENO);	
            		if (status<0){
            			perror("dup2 in c->out");
            			exit(0);
            		}
            		
            		close(pipefd[1]);
                }

                if ( c->in == Tpipe ) {
                	close(oldpipefd[1]);
                }

                if ( c->out == Tpipe ) {
                	close(pipefd[0]);
                }

                // execute the command
                execvp(c->args[0], c->args);
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
                if ( c->in == Tpipe ) {
                	close(oldpipefd[0]);
                	//fprintf(stderr,"%d closed\n",oldpipefd[0]);
            	}

            	if ( c->out == Tpipe ) {
                	oldpipefd[0] = pipefd[0];
		            oldpipefd[1] = pipefd[1];
		            close(pipefd[1]);
		            //fprintf(stderr,"%d closed\n",oldpipefd[1]);
		            //fprintf(stderr,"pipes copied\n");
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
    close(pipefd[0]);
    close(pipefd[1]);
    fprintf(stderr,"End pipe\n");
    execPipe(p->next);
}

int main(int argc, char *argv[])
{
    Pipe p;
    char *host;
    host = getenv("USER");

    while ( 1 ) {
        fprintf(stderr,"%s%% ", host);
        p = parse();
        execPipe(p);
        freePipe(p);
    }
}

/*........................ end of main.c ....................................*/
