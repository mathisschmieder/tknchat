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


int main(int argc, char** argv) {
  parse_options(argc, argv);
  
  srand( time(NULL) ); //maybe take a better seed
  OS_Level = rand() % 65535 + 1;

  if (eth == NULL) 
    eth = "eth0"; 

  MyClientCredentials.sockaddr = getIP(eth);
  MyClientCredentials.name[1023] = '\0';
  if (nick == NULL)
    gethostname(MyClientCredentials.name, 1023);
  else
    strncpy(MyClientCredentials.name, nick, sizeof(MyClientCredentials.name));

  sd = setup_multicast();
  init_fdSet(&rfds);

  close(sd);
  return 0;
}

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
sockaddr_in * getIP(const char* eth) {
  struct ifaddrs * ifAddrStruct=NULL;
  struct ifaddrs * ifa=NULL;
  struct sockaddr_in * sockaddr;

  getifaddrs(&ifAddrStruct);
 
  for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
    if ((ifa->ifa_addr != NULL) && (ifa->ifa_addr->sa_family==AF_INET) && (strcmp(ifa->ifa_name, eth) == 0)) {
        sockaddr = (struct sockaddr_in *)(ifa->ifa_addr);
       break;
    }
  }
  freeifaddrs(ifAddrStruct);

  return sockaddr;
}

int init_fdSet(fd_set* fds) {

  // initialize fds structure
  FD_ZERO(fds);

  // add console filedescriptor
  FD_SET(0, fds);

  // add multicast filedescriptor
  FD_SET(sd, fds);

}

int setup_multicast() {
  int sd;
  struct ip_mreq group;

  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sd < 0) {
    perror("Error opening datagram socket");
    exit(1);
  }
 
  int reuse = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
    perror("Error setting SO_REUSEADDR");
    exit(1);
  }

  memset((char* ) &msock, 0, sizeof(msock));
  msock.sin_family = AF_INET;
  msock.sin_port = htons(MC_GROUP_PORT);
  msock.sin_addr.s_addr = inet_addr(MC_GROUP_ADDR);

  if(bind(sd, (struct sockaddr*)&msock, sizeof(msock))) {
    perror("Error binding datagram socket");
    exit(1);
  }

  group.imr_multiaddr.s_addr = inet_addr(MC_GROUP_ADDR);
  group.imr_interface.s_addr = INADDR_ANY;
  if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
     perror("Error joining multicast");
     exit(1);
   }

  return sd;
}

int send_multicast(char* data) {
  return sendto(sd, data, sizeof(data), 0, (struct sockaddr*)&msock, sizeof(msock));
}






















