#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

static void execCmd(Cmd c)
{
  int i;
  pid_t childpid;
  if ( c ) {
    printf("%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);
    if ( c->in == Tin )
      printf("<(%s) ", c->infile);
    if ( c->out != Tnil )
      switch ( c->out ) {
      case Tout:
	printf(">(%s) ", c->outfile);
	break;
      case Tapp:
	printf(">>(%s) ", c->outfile);
	break;
      case ToutErr:
	printf(">&(%s) ", c->outfile);
	break;
      case TappErr:
	printf(">>&(%s) ", c->outfile);
	break;
      case Tpipe:
	printf("| ");
	break;
      case TpipeErr:
	printf("|& ");
	break;
      default:
	fprintf(stderr, "Shouldn't get here\n");
	exit(-1);
      }

    if ( c->nargs > 1 ) {
      printf("[");
      for ( i = 1; c->args[i] != NULL; i++ )
	printf("%d:%s,", i, c->args[i]);
      printf("\b]");
    }
    putchar('\n');
    // this driver understands one command
    int returncode;
    childpid = fork();
    switch(childpid){
    	case(0):
    		execvp(c->args[0], c->args);
    		// error is executable/command not found 
    		if (errno == 2) 
    			fprintf(stderr, "command not found\n");
    		// error is permission denied
    		else if (errno == 13)
    			fprintf(stderr, "permission denied\n");
			else
				fprintf(stderr, "%d: %s\n", errno,strerror(errno));
    		exit(EXIT_FAILURE);
    	case(-1):
    		fprintf(stderr, "Fork error\n");
    		exit(EXIT_FAILURE);
    	default:
    		wait(NULL);
	}

    if ( !strcmp(c->args[0], "end") || !strcmp(c->args[0], "exit"))
      exit(0);
  }
}

static void execPipe(Pipe p)
{
  int i = 0;
  Cmd c;

  if ( p == NULL )
    return;

  printf("Begin pipe%s\n", p->type == Pout ? "" : " Error");
  for ( c = p->head; c != NULL; c = c->next ) {
    printf("  Cmd #%d: ", ++i);
    execCmd(c);
  }
  printf("End pipe\n");
  execPipe(p->next);
}

int main(int argc, char *argv[])
{
  Pipe p;
  char *host;
  host = getenv("USER");

  while ( 1 ) {
    printf("%s%% ", host);
    p = parse();
    execPipe(p);
    freePipe(p);
  }
}

/*........................ end of main.c ....................................*/