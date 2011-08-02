/*
** Technische Universitaet Berlin
** Lehrstuhl fuer Kommunikationsnetze
**
** Praktikum Kommunikationsnetze
** Block D - Chattool
**
** Mathis Schmieder - 316110
** Konstantin Koslowski - 316955
*/

#include "tknchat.h"

// argument parse 
int option_index = 0;
const char* eth = NULL; 
const char* nick = NULL; 

fd_set rfds;

int appl_state;
struct timeval globalTimer;

char buf[MAX_MSG_LEN];
int buflen;
int OS_Level = 0;

ClientCredentials MyClientCredentials;

sockaddr_in * getIP(const char*);

void parse_options(int argc, char** argv) {
  
  int ret;
  
  while ((ret = getopt_long(argc,argv,"hvi:n:",
			    long_options, &option_index)) != EOF)
    switch (ret) {
    case 'h':
      //usage(argv[0]);
      printf("usage\n");
      break;               
    case 'v':
      //version(argv[0]);
      printf("version\n");
      break;               
    case 'i':
      //printf("interface: %s\n", optarg);
      eth = optarg;
      break;               
    case 'n':
      //printf("nick: %s\n", optarg);
      nick = optarg;
      break;               
    }

}

int main(int argc, char** argv) {
  parse_options(argc, argv);
  
  srand( time(NULL) ); //maybe take a better seed
  OS_Level = rand() % 65535 + 1;

  if (eth 

