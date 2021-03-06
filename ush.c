/*
Author: Siddharth Sharma, NCSU
2016
*/

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
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

int pipefd[2];
int oldpipefd[2];
int status, interactive, shell_terminal, forks;
int fdin, fdout, prev_in, prev_out, prev_err;
struct sigaction sa;
char home_config_path[1000];
int jobs[1000];
extern char** environ;
pid_t shell_pgid,pipe_group;

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
int handle_bg(int, char **);
int handle_fg(int, char **);
int find_config();
int count_lines(FILE*);

inbuilt_cmd inbuilt_cmds[] = {
  {"getenv", handle_getenv},
  {"setenv", handle_setenv},
  {"unsetenv", handle_unsetenv},
  {"nice",   handle_nice},
  {"fg",     handle_fg},
  {"bg",     handle_bg},
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

int handle_bg(int argc, char ** args)
{
    printf("bg: No current job.\n");
    return 0;
}

int handle_fg(int argc, char ** args)
{
    printf("fg: No current job.\n");
    return 0;
}

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
	int status,num;
    pid_t child;
	if ( argc < 3 ) {
		if ( argc < 2 ) {
            status = setpriority(PRIO_PROCESS, 0, 4);    
        }
        else {
            num = (int) strtol(args[1], (char **)NULL, 10);
            //fprintf(stderr,"priority num: %d\n", num);
            status = setpriority(PRIO_PROCESS, 0, num);
        }
	}
	else {
        num = (int) strtol(args[1], (char **)NULL, 10);
        child = fork();
        switch( child ) {
			case 0:
				//input is "nice <number> <cmd>"
				//child execute "nice -n <number> <cmd>"
				execvp(args[2], args+2);
			case -1:
				//error
				perror("nice-fork");
		        return -1;
		    default:
		    	//parent
		    	status = setpriority(PRIO_PROCESS, child, num);
                wait(NULL);
                if ( status>0 ){
                    return 0;
                }
                return -1;       
		}

	}
    if ( status < 0 ) {
        perror("nice");
        return -1;
    }

	return 0;	
}


int is_file(const char* path) {
    struct stat buf;
    stat(path, &buf);
    return S_ISREG(buf.st_mode);
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
        }
    }

    char *path = getenv("PATH");
    char *item = NULL;
    int found  = 0;

    if (!path) {
        return -1;
    }

    path = strdup(path);

    char real_path[4096]; // or PATH_MAX or something smarter
    for (item = strtok(path, ":"); item; item = strtok(NULL, ":")){
        sprintf(real_path, "%s/%s", item, args[1]);
        if ( is_file(real_path) && !(
               access(real_path, F_OK) 
            || access(real_path, X_OK))) // check if the file exists and is executable
        {
            printf("%s\n",real_path);
        }
    }

    free(path);
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
	}

    else if(argc<3){
    	args[2] = "NULL";
    }

    int status;
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
    var = getenv(args[1]);
    if ( var == NULL ) {
        //fprintf(stderr,"cannot find %s\n", args[1]);
        return -1;
    }
    printf("%s\n",var);
    return 0;
}

static void execCmd(Cmd c, int first)
{
    int i, foreground=1;
    pid_t childpid;
    
    if ( c->exec == Tamp ) {
        foreground = 0;
    }
    
    if ( c ) {
        if ( c->next == NULL ) {   
		    for(i = 0; i < sizeof(inbuilt_cmds)/sizeof(*inbuilt_cmds); i++) {
		        if( strcmp(c->args[0],inbuilt_cmds[i].cmd)==0 ) {
		            if( inbuilt_cmds[i].handler != NULL ) {
		                
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

                            //save stdout
                            prev_out = dup(STDOUT_FILENO);
                            fcntl(prev_out, FD_CLOEXEC);
                            dup2(fdout, STDOUT_FILENO);
                            
                            //check for stderr
                            if ( c->out == ToutErr || c->out == TappErr ) {
                                prev_err = dup(STDERR_FILENO);
                                fcntl(prev_err, FD_CLOEXEC);
                                dup2(fdout, STDERR_FILENO);
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
                            dup2(fdin, STDIN_FILENO);
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
		            
                    for ( i = 0; i < forks;i++ ) {
                        wait(NULL);
                    }
                    
                    if (interactive == 1) {
                        tcsetpgrp (shell_terminal, shell_pgid);    
                    } 
                    
                    return;
		        }
		    }
        }        
        //handle pipe
        if ( c->out == Tpipe || c->out == TpipeErr ){ 
        	pipe(pipefd);
        }

        forks+=1;
        childpid = fork();
        switch ( childpid ) {
            case 0:
            	if ( interactive == 1 ) {
                    //setting process groups
                    pid_t pgid;
                    childpid = getpid ();
                    if (first == 1){ 
                        pgid = childpid;
                    }
                    else {
                        pgid = pipe_group;
                    }
                    setpgid (childpid, pgid);
                    //if ending with a & block output and input
                    if ( foreground == 1 ){
                        tcsetpgrp (shell_terminal, pgid);
                    }
                    else {
                        if (c->in == 12) {
                            c->in = Tin;
                            c->infile = "/dev/null";
                        }
                        if (c->out == 12) {
                            c->out = Tout;
                            c->outfile = "/dev/null";
                        }
                    }
                    
                    // set handler to default behaviour
                    sa.sa_handler = SIG_DFL;
                    status = sigaction(SIGINT, &sa, NULL);
                    status = sigaction(SIGQUIT, &sa, NULL);
                    status = sigaction(SIGTERM, &sa, NULL);
    			    status = sigaction(SIGTTIN, &sa, NULL);
                    status = sigaction(SIGTTOU, &sa, NULL);
                    status = sigaction(SIGCHLD, &sa, NULL);
                    status = sigaction(SIGTSTP, &sa, NULL);
                }
                
                //deal with input redirect
                if ( c->in == Tin ) {
                    fdin = open(c->infile,O_RDONLY);
                    if ( fdin < 0 ) {
                        perror("open");
                        exit(0);
                    }
                    
                    dup2(fdin, STDIN_FILENO);
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
                        perror("open");
                        exit(0);
                    }
                    
                    dup2(fdout, STDOUT_FILENO);
                    
                    if ( c->out == ToutErr || c->out == TappErr ) {
                        dup2(fdout, STDERR_FILENO);
                    }

                    close(fdout);
                }

                // deal with pipes and pipeErr redirects
                if ( c->in == Tpipe || c->in == TpipeErr ) {
                	dup2(oldpipefd[0], STDIN_FILENO);
                	close(oldpipefd[0]);
                }

                if ( c->out == Tpipe || c->out == TpipeErr ) {
            		dup2(pipefd[1],  STDOUT_FILENO);	
                    if ( c->out == TpipeErr ) {
                        dup2(pipefd[1],  STDERR_FILENO);      
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
                if ( interactive == 1 ) {
                    if (first == 1){ 
                        pipe_group = childpid;
                    }
                    else {
                        if ( getpgid(childpid) != pipe_group ) {
                            setpgid(childpid, pipe_group);
                        }
                    }
                }
                
                if (c->next==NULL){
                    for ( i=0; i<forks;i++ ) {
                        wait(NULL);
                    }
                    tcsetpgrp (shell_terminal, shell_pgid);
                }
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

    forks = 0;
    for ( c = p->head; c != NULL; c = c->next ) {
        if ( c == p->head )
            execCmd(c, 1);    
        else
            execCmd(c, 0);
    }
    execPipe(p->next);
}

int main(int argc, char *argv[])
{
    Pipe p;
    char host[1024];

    //see if interaactive or input from file
    shell_terminal = STDIN_FILENO;
    interactive = isatty(STDIN_FILENO);

    //set buffers off
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    //handle .ushrc
    if ( find_config() == 0 ) {
        int lines, config;
        
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
    if ( interactive ==1 ) {
        //check if ush is not in background
        while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
            kill (- shell_pgid, SIGTTIN);

        //handle signals
        sa.sa_handler = SIG_IGN;
        status = sigaction(SIGINT, &sa, NULL);
        status = sigaction(SIGQUIT, &sa, NULL);
        status = sigaction(SIGTERM, &sa, NULL);
        status = sigaction(SIGTSTP, &sa, NULL);
        status = sigaction(SIGTTIN, &sa, NULL);
        status = sigaction(SIGTTOU, &sa, NULL);
        status = sigaction(SIGCHLD, &sa, NULL);
        
        //put shell into its own process group
        shell_pgid = getpid ();
        setpgid (shell_pgid, shell_pgid);
        
        /* Grab control of the terminal.  */
        tcsetpgrp (shell_terminal, shell_pgid);
        
        //get hostname
        gethostname(host, 1023);    
    }
    
    while ( 1 ) {
        if ( interactive == 1 )  {
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
