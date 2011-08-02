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
int retval;
mc_packet request;

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
  
  request.type = MC_REQUEST_MEMBERSHIP;
  send_multicast(request);

  setNewState(STATE_INIT);

  globalTimer.tv_sec = 1;
  globalTimer.tv_usec = 0;

  for (;;) { // main loop
    
    retval = select(sizeof(&rfds)*8, &rfds, NULL, NULL, (struct timeval*)&globalTimer);
    if (FD_ISSET(sd, &rfds)) {
      mc_packet fnord;
      recv(sd, &fnord, sizeof(fnord), 0);
    }
    init_fdSet(&rfds);
    setGlobalTimer(1,0);
  }


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

  char loopch = 0;
  if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopch, sizeof(loopch)) < 0) {
    perror("Error setting IP_MULTICAST_LOOP");
    close(sd);
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

int send_multicast(mc_packet data) {
  return sendto(sd, (char *)&data, sizeof(data), 0, (struct sockaddr*)&msock, sizeof(msock));
}


void setNewState(int state) {
  appl_state = state;
}

int getState() {
  return appl_state;
}

void setGlobalTimer(int sec, int usec) {
  static int i_sec;
  static int i_usec;

  if ((globalTimer.tv_sec == -1) && (globalTimer.tv_usec == -1))
    {
      printf("1\n");
      globalTimer.tv_sec  = i_sec;
      globalTimer.tv_usec = i_usec;
      return;
    }

  i_sec  = sec;
  i_usec = usec;

  if (sec <= globalTimer.tv_sec)
    if (usec <= globalTimer.tv_usec)
       printf("2\n");
  {
	globalTimer.tv_sec  = sec;
	globalTimer.tv_usec = usec;
      }

  if ((globalTimer.tv_sec == 0) && (globalTimer.tv_usec == 0))
    {      printf("3\n");
      globalTimer.tv_sec  = sec;
      globalTimer.tv_usec = usec;
    }
}

















