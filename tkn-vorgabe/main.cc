//----------------------------------------------------------------------
//  Technische Universität Berlin
//  Lehrstuhl für Telekommunikationsnetze
//
//  1231 L 303 Praktikum Kommunikationsnetze
//  Block D
//
//  Copyright 2001 Andreas Köpsel
//
//----------------------------------------------------------------------
//  CHAT-Programm Grundgerüst
//  


#include "main.h"  


int             option_index          = 0;
int             verbose               = 0;

fd_set          rfds;

int             appl_state;
struct timeval  globalTimer;

char            buf[MAX_MSG_LEN];
int             buflen;

void            setGlobalTimer(int, int);
void            clearGlobalTimer(void);
void            setNewState(int);
int             getApplState(void);




int main(int argc, char** argv) {

  int retval;
  int fd_val;
  int rc;
  int maxsockfd = 0;
  int state_count = 0;

  setNewState(STATE_INIT);

  parse_options(argc, argv);

  signal(SIGINT,  abort_chat);
  signal(SIGTERM, abort_chat);
  signal(SIGUSR1, db_dump);

  globalTimer.tv_sec  = 1;
  globalTimer.tv_usec = 0;


  
  init_fdSet(&rfds);
  
  maxsockfd = find_highest_fd() + 1;
  
  
  for (;;)
    {

	  // if globalTimer set, select with upper bound, else indefinitely long waiting
	  if ((globalTimer.tv_sec != 0) || (globalTimer.tv_usec != 0)) 
	    {
	      retval = select(maxsockfd, &rfds, (fd_set*)NULL, (fd_set*)NULL, (struct timeval*)&globalTimer);
	    }
	  else
	    {
	      retval = select(maxsockfd, &rfds, (fd_set*)NULL, (fd_set*)NULL, (struct timeval*)NULL);
	    }




     	  assert(retval >= 0);

	  
	  if (retval == 0)
	    {
	      
	      // Zustandsmaschine ChatTool
	      // switch (appl_state)
	      // case STATE_...:
	      //           do whatever is necessary
	      //           break;

	    }
	  else 
	    {
	      
	      for (fd_val = 0; retval--; fd_val++) {  
		
		while (!FD_ISSET(fd_val, &rfds))
		  fd_val++;  
		
		// console stdin
		if (fd_val == 0)
		  {
		    
		    // read line from stdin

		  }
		    
	      }


	    }

	  // restore rfds
	  init_fdSet(&rfds);
	  maxsockfd = find_highest_fd() + 1;

    }



  // just to make gcc happy
  return 0;

}

void error_handler (long error) {

   abort_chat(0);

}

void abort_chat(int status)
{


}

void db_dump(int status)
{



}

void version(char* progname)
{

  fprintf(stderr,"%s version 0.01\n",progname);

}

void usage(char* progname)
{

  fprintf(stderr,"usage: %s\n\t[-h|--help]\n\t[-V|--version]\n",progname);

}

void parse_options(int argc, char** argv) {
  
  int ret;
  
  while ((ret = getopt_long(argc,argv,"p:vVhs:l:",
			    long_options, &option_index)) != EOF)
    switch (ret) {
    case 'h':
      usage(argv[0]);
      break;               
    case 'V':
      version(argv[0]);
      break;               
    }

}








void
setGlobalTimer(int sec, int usec)
{

  static int i_sec;
  static int i_usec;

  if ((globalTimer.tv_sec == -1) && (globalTimer.tv_usec == -1))
    {
      //fprintf(stderr,"setting timer (2)\n");
      globalTimer.tv_sec  = i_sec;
      globalTimer.tv_usec = i_usec;
      return;
    }

  i_sec  = sec;
  i_usec = usec;

  if (sec <= globalTimer.tv_sec)
    if (usec <= globalTimer.tv_usec)
      {
	//fprintf(stderr,"setting timer (1)\n");
	globalTimer.tv_sec  = sec;
	globalTimer.tv_usec = usec;
      }

  if ((globalTimer.tv_sec == 0) && (globalTimer.tv_usec == 0))
    {
      //fprintf(stderr,"setting timer (2)\n");
      globalTimer.tv_sec  = sec;
      globalTimer.tv_usec = usec;
    }


}











void
clearGlobalTimer()
{
  globalTimer.tv_sec   = 0;
  globalTimer.tv_usec  = 0;
}










void
setNewState(int newState)
{

  appl_state = newState;

}






int
getApplState()
{

  return appl_state;

}







int
find_highest_fd()
{
  
  int fd = 0;

  
  // add additional file descriptors for data and net sockets


  return fd;

}








int
init_fdSet(fd_set* fds)
{

  // initialize fds structure
  FD_ZERO(fds);

  // add console filedescriptor
  FD_SET(0, fds);

}
