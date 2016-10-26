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
int fdin, fdout, prev_in, prev_out, prev_err;
struct sigaction sa;
char home_config_path[1000];
extern char** environ;

typedef struct {
  char *cmd;
  int (*handler)(int, char **);
} inbuilt_cmd;

int handle_unsetenv(int, char **);
int handle_setenv(int, char **);
int handle_getenv(int, char **);
int handle_exit(int, char **);
int handle_cd(int, char **);
int handle_where(int, char **);
int handle_nice(int, char **);
int handle_pwd(int, char **);
int handle_echo(int, char **);
int find_config();
int count_lines(FILE*);

inbuilt_cmd inbuilt_cmds[] = {
  {"getenv", handle_getenv},
  {"setenv", handle_setenv},
  {"unsetenv", handle_unsetenv},
  {"nice",   handle_nice},
  {"fg",     NULL},
  {"bg",     NULL},
  {"kill",	 NULL},
  {"jobs", 	 NULL},
  {"echo",   handle_echo},
  {"pwd",    handle_pwd},
  {"cd",     handle_cd},
  {"where",  handle_where},
  {"end",    handle_exit},
  {"exit",   handle_exit},
  {"logout", handle_exit}
};

int handle_echo(int argc, char ** args) 
{
    int i=1;
    while ( i < argc ) {
        printf("%s", args[i]);
        if ( i==argc-1 ) {
            printf("\n");
        }
        else {
            printf(" ");
        }
        i+=1;
    }
    return 0;
}

int handle_pwd(int argc, char ** args)
{
    char *result = realpath(".", NULL);
    if ( result != NULL ) {
        printf("%s\n", result);
        free(result);
        return 0;
    }
    return -1;
}

int handle_exit(int argc, char ** args)
{
    exit(0);
}

int handle_nice(int argc, char ** args)
{
	int status;
	
	if ( argc < 2 ) {
		status = nice(4);
		if ( status < 0 ) {
			perror("handle_nice");
			exit(0);
		}
	}
	else if ( argc < 3 ) {
		int num = (int) strtol(args[1], (char **)NULL, 10);
		fprintf(stderr,"priority num: %d\n", num);
		status = nice(num);
		if ( status < 0 ) {
			perror("handle_nice");
			exit(0);
		}
	}
	else {
		status = fork();

		switch( status ) {
			case 0:
				//input is "nice <number> <cmd>"
				//child execute "nice -n <number> <cmd>"
				args[3] = args[2];
				args[2] = args[1];
				args[1] = "-n";
				execvp(args[0], args);
			case -1:
				//error
				fprintf(stderr, "Fork error\n");
		        exit(EXIT_FAILURE);
		    default:
		    	//parent
		    	wait(NULL);
		}
	}

	return 0;	
}

int handle_where(int argc, char **args) 
{
	if( argc < 2 ){
		return -1;
    }

	int i;
	for(i = 0; i < sizeof(inbuilt_cmds)/sizeof(*inbuilt_cmds); i++) {
        if( strcmp(args[1],inbuilt_cmds[i].cmd)==0 ) {
            printf("%s: shell built-in command.\n", args[1]);
            return 0;
        }
    }

    int status = fork();

    switch( status ) {
    	case 0:
    		//child execute "which -a <cmd>"
    		args[0] = "which";
    		args[2] = args[1];
    		args[1] = "-a";
    		execvp(args[0], args);
    	case -1:
    		//error
    		fprintf(stderr, "Fork error\n");
            exit(EXIT_FAILURE);
        default:
        	//parent
        	wait(NULL);
    }
	return 0;
}

int handle_cd(int argc, char **args)
{
    int status;
    if(argc<2){
    	args[1] = getenv("HOME");
    }
    
    status = chdir(args[1]);
    if ( status < 0 ) {
        perror("cd");
        return -1;
    }
    return 0;
}

int handle_setenv(int argc, char **args)
{
    if(argc<2){
        int i = 0;
        while(environ[i]) {
            printf("%s\n", environ[i++]);
        }
        return 0;
		// int status = fork();

	 //    switch( status ) {
	 //    	case 0:
	 //    		//child exec env
	 //    		args[0] = "env";
	 //    		execvp(args[0], args);
	 //    	case -1:
	 //    		//error
	 //    		fprintf(stderr, "Fork error\n");
	 //            exit(EXIT_FAILURE);
	 //        default:
	 //        	//parent
	 //        	wait(NULL);
	 //        	return 0;
	 //    }
		
    }

    else if(argc<3){
    	args[2] = "NULL";
    }

    int status;
    //todo handle error when args doesn't have 1 and 2
    status = setenv(args[1],args[2],1);
    if ( status < 0 ) {
        perror("setenv");
        return -1;
    }
    return 0;
}

int handle_unsetenv(int argc, char **args)
{
    if(argc<2){
		return -1;
    }

    int status;
    //todo handle error when args doesn't have 1
    status = unsetenv(args[1]);
    if ( status < 0 ) {
        perror("unsetenv");
        return -1;
    }
    return 0;
}

int handle_getenv(int argc, char **args)
{   
    if(argc<2){
		return -1;
    }

    char *var;
    //todo handle error when args doesn't have 1
    var = getenv(args[1]);
    if ( var == NULL ) {
        fprintf(stderr,"cannot find %s\n", args[1]);
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
        /*fprintf(stderr,"%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
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
        */
        // this driver understands one command

        //check for builtin commands
        // if last command then execute in parent shell
		//if ( c->out != Tpipe && c->out != TpipeErr && c->in != Tpipe && c->in != TpipeErr) {   
        if ( c->next == NULL ) {   
		    for(i = 0; i < sizeof(inbuilt_cmds)/sizeof(*inbuilt_cmds); i++) {
		        if( strcmp(c->args[0],inbuilt_cmds[i].cmd)==0 ) {
		            if( inbuilt_cmds[i].handler != NULL ) {
		                
                        //deal with output and error redirect
                        if ( c->out == Tout || c->out == Tapp || c->out == ToutErr || c->out == TappErr ) {
                            //open file in append mode
                            if ( c->out == Tapp || c->out == TappErr ) {
                                fdout = open(c->outfile, O_WRONLY|O_CREAT|O_APPEND, 0666);
                            }
                            //open file in truncate mode
                            else {
                                fdout = open(c->outfile, O_WRONLY|O_CREAT|O_TRUNC, 0666);    
                            }

                            if ( fdout < 0 ) {
                                perror("open error");
                                exit(0);
                            }

                            //save stdout
                            prev_out = dup(STDOUT_FILENO);
                            fcntl(prev_out, FD_CLOEXEC);
                            
                            status = dup2(fdout, STDOUT_FILENO);
                            if ( status < 0 ) {
                                perror("dup2");
                                exit(0);   
                            }
                            
                            //check for stderr
                            if ( c->out == ToutErr || c->out == TappErr ) {
                                prev_err = dup(STDERR_FILENO);
                                fcntl(prev_err, FD_CLOEXEC);

                                status = dup2(fdout, STDERR_FILENO);
                                if ( status < 0 ) {
                                    perror("dup2");
                                    exit(0);   
                                }    
                            }

                            close(fdout);

                        }

                        //deal with input redirect
                        if ( c->in == Tin ) {
                            fdin = open(c->infile,O_RDONLY);
                            if ( fdin < 0 ) {
                                perror("open error");
                                exit(0);
                            }
                            
                            //save stdout
                            prev_in = dup(STDIN_FILENO);
                            fcntl(prev_in, FD_CLOEXEC);
                            
                            status = dup2(fdin, STDIN_FILENO);
                            if ( status < 0 ) {
                                perror("dup2");
                                exit(0);   
                            }
                            close(fdin);
                        }

                        //execute command
                        inbuilt_cmds[i].handler(c->nargs, c->args);

                        //restore stdout and stderr
                        if ( c->out == Tout || c->out == Tapp || c->out == ToutErr || c->out == TappErr) {
                            dup2(prev_out, STDOUT_FILENO);
                            close(prev_out);

                            //check for stderr
                            if( c->out == ToutErr || c->out == TappErr ) {
                                dup2(prev_err, STDERR_FILENO);
                                close(prev_err);
                            }   
                        }

                        //restore stdin
                        if ( c->in == Tin ) {
                            dup2(prev_in, STDIN_FILENO);
                            close(prev_in);
                        }
		            }
		            return;
		        }
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
                //handle sigterm child doesn't ignore
			    status = sigaction(SIGTERM, &sa, NULL);
			    if ( status == -1 ) {
			        perror("Error: cannot handle SIGTERM");
			    }
			    //handle sigtstp cntl+"z" child doesn't ignore
			    status = sigaction(SIGTSTP, &sa, NULL);
			    if ( status == -1 ) {
			        perror("Error: cannot handle SIGTSTP");
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

                //deal with output and error redirect
                if ( c->out == Tout || c->out == Tapp || c->out == ToutErr || c->out == TappErr ) {
                    if ( c->out == Tapp || c->out == TappErr ) {
                        fdout = open(c->outfile, O_WRONLY|O_CREAT|O_APPEND, 0666);
                    }

                    else {
                        fdout = open(c->outfile, O_WRONLY|O_CREAT|O_TRUNC, 0666);    
                    }

                    if ( fdout < 0 ) {
                        perror("open error");
                        exit(0);
                    }
                    
                    status = dup2(fdout, STDOUT_FILENO);
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);   
                    }
                    
                    if ( c->out == ToutErr || c->out == TappErr ) {
                        status = dup2(fdout, STDERR_FILENO);
                        if ( status < 0 ) {
                            perror("dup2");
                            exit(0);   
                        }    
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

                if ( c->out == Tpipe || c->out == TpipeErr ) {
            		status = dup2(pipefd[1],  STDOUT_FILENO);	
                    if ( status < 0 ) {
                        perror("dup2");
                        exit(0);
                    }

                    if ( c->out == TpipeErr ) {
                        status = dup2(pipefd[1],  STDERR_FILENO);      
                        if ( status < 0 ) {
                            perror("dup2");
                            exit(0);
                        }
                    }

                    close(pipefd[1]);
                }

                if ( c->in == Tpipe || c->in == TpipeErr ) {
                	close(oldpipefd[1]);
                }

                if ( c->out == Tpipe || c->out == TpipeErr ) {
                	close(pipefd[0]);
                }
                
                // execute builtin command
                for(i = 0; i < sizeof(inbuilt_cmds)/sizeof(*inbuilt_cmds); i++) {
                    if( strcmp(c->args[0],inbuilt_cmds[i].cmd)==0 ) {
                        if( inbuilt_cmds[i].handler != NULL ) {
                            inbuilt_cmds[i].handler(c->nargs, c->args);
                            exit(0);
                        }
                    }
                }
                
                //exec
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

    //fprintf(stderr,"Begin pipe%s\n", p->type == Pout ? "" : " Error");
    for ( c = p->head; c != NULL; c = c->next ) {
    //    fprintf(stderr,"  Cmd #%d: ", ++i);
        execCmd(c);
    }
    //fprintf(stderr,"End pipe\n");
    execPipe(p->next);
}

int main(int argc, char *argv[])
{
    int output;
    Pipe p;
    char host[1024];

    //set buffers off
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    //handle signals
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
    //handle sigterm parent ignores
    status = sigaction(SIGTERM, &sa, NULL);
    if ( status == -1 ) {
        perror("Error: cannot handle SIGTERM");
    }
    //handle sigtstp cntl+"z" parent ignores
    status = sigaction(SIGTSTP, &sa, NULL);
    if ( status == -1 ) {
        perror("Error: cannot handle SIGTSTP");
    }

    //handle .ushrc
    if ( find_config() == 0 ) {
        int lines, config;
        fprintf(stderr, ".ushrc found!\n");
        
        //open file and read lines
        FILE* conf = fopen(home_config_path,"r");
    	lines = count_lines(conf);
        fclose(conf);
        
        //redirect file to stdin
        config = open(home_config_path,O_RDONLY);
    	prev_in = dup(STDIN_FILENO);
        fcntl(prev_in, FD_CLOEXEC);
		dup2(config, STDIN_FILENO);
        close(config);
        
		//run execute loop
        while( lines > 0 ) {
      		p = parse();
      		execPipe(p);
      		freePipe(p);
      		lines-=1;
      	}

      	//restore stdin
    	dup2(prev_in, STDIN_FILENO);
    	close(prev_in);	
    }

	//normal shell
    //host = getenv("USER");
    gethostname(host, 1023);
    output = isatty(STDIN_FILENO);
    while ( 1 ) {
        if ( output==1 )  {
            fprintf(stdout,"%s%% ", host);
        }
        p = parse();
        execPipe(p);
        freePipe(p);
    }
}

int find_config(){
	int status;
	strcpy(home_config_path, getenv("HOME"));
	strcat(home_config_path, "/");
	strcat(home_config_path, ".ushrc");
	return access(home_config_path, F_OK );
	//returns 0 if success -1 if failure
}

int count_lines(FILE* f) {
	int lines = 0;
	int ch = 0;
  	while(!feof(f)){
  		ch = fgetc(f);
  		if(ch == '\n') {
    		lines++;
  		}
	}

	return lines;
}


/*........................ end of main.c ....................................*/
