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

sockaddr_in * getIP(const char*);

int main(int argc, char** argv) {
  srand( time(NULL) ); //maybe take a better seed
  OS_Level = rand() % 65535 + 1;

  const char* eth = "eth0"; //TODO: get passed argument
  MyClientCredentials.sockaddr = getIP(eth);
  MyClientCredentials.name[1023] = '\0';
  gethostname(MyClientCredentials.name, 1023);

  printf("My IP: %s and my nick: %s\n", inet_ntoa(MyClientCredentials.sockaddr->sin_addr), MyClientCredentials.name);



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
