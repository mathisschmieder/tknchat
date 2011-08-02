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

void getIP(const char*);
char ip_addr[INET_ADDRSTRLEN];
in_addr_t in_addr; //dont know if needed

int main(int argc, char** argv) {
  srand( time(NULL) ); //maybe take a better seed
  OS_Level = rand() % 65535 + 1;

  const char* eth = "eth0"; //TODO: get passed argument
  getIP(eth);



  return 0;
}

void getIP(const char* eth) {
  struct ifaddrs * ifAddrStruct=NULL;
  struct ifaddrs * ifa=NULL;
  void * if_addr=NULL;

  getifaddrs(&ifAddrStruct);
 
  for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
    if ((ifa->ifa_addr != NULL) && (ifa->ifa_addr->sa_family==AF_INET) && (strcmp(ifa->ifa_name, eth) == 0)) {
        if_addr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
    }
  }
  inet_ntop(AF_INET, if_addr, ip_addr, INET_ADDRSTRLEN);
  in_addr = inet_addr(ip_addr);
}
