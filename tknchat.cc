/*
** Technische Universitaet Berlin
** Lehrstuhl fuer Kommunikationsnetze
**
** Praktikum Kommunikationsnetze
** Block D - Chattool
**
** Mathis Schmieder - 316110
** Konstantin Koslowski - 316955 <-- is die richtig?
*/

#include "tknchat.h"

fd_set rfds;

int appl_state;
struct timeval globalTimer;

char buf[MAX_MSG_LEN];
int buflen;
int OS_Level = 0;

ClientCredentials MyClientCredentials;


int main(int argc, char** argv) {
  srand( time(NULL) ); //maybe take a better seed
  OS_Level = rand() % 65535 + 1;

  const char* eth = "eth0"; //TODO: get passed argument
  MyClientCredentials.sockaddr = getIP(eth);
  MyClientCredentials.name[1023] = '\0';
  gethostname(MyClientCredentials.name, 1023);

  init_fdSet(&rfds);

  sd = setup_multicast();

  close(sd);
  return 0;
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
























