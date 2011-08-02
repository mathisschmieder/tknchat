/* Send Multicast Datagram code example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>
#include <ifaddrs.h>

#define MC_GROUP_ADDR "225.2.0.1"
#define MC_GROUP_PORT 5020

struct in_addr localInterface;
struct sockaddr_in groupSock;
int sd;
char databuf[1024];
int datalen = sizeof(databuf);
 
int main (int argc, char *argv[ ])
{
  /* Create a datagram socket on which to receive. */
  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sd < 0)
  {
  perror("Opening datagram socket error");
  exit(1);
  }
  else
  printf("Opening datagram socket....OK.\n");
 
  int reuse = 1;
  if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
  {
    perror("Setting SO_REUSEADDR error");
    exit(1);
  }
  else
  printf("Setting SO_REUSEADDR...OK.\n");

  memset((char *) &groupSock, 0, sizeof(groupSock));
  groupSock.sin_family = AF_INET;
  groupSock.sin_port = htons(MC_GROUP_PORT);
  groupSock.sin_addr.s_addr = inet_addr(MC_GROUP_ADDR);

/* Disable loopback so you do not receive your own datagrams. */
{
char loopch = 0;
if(setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
{
perror("Setting IP_MULTICAST_LOOP error");
close(sd);
exit(1);
}
else
printf("Disabling the loopback...OK.\n");
}
 
struct ifaddrs * ifAddrStruct=NULL;
struct ifaddrs * ifa=NULL;
void * if_addr=NULL;

const char* eth = "eth0";

printf("getif: %d\n", getifaddrs(&ifAddrStruct));

for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
    if ((ifa->ifa_addr != NULL) && (ifa->ifa_addr->sa_family==AF_INET) && (strcmp(ifa->ifa_name, eth) == 0)) {             
        if_addr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
    }
}

char addressBuffer[INET_ADDRSTRLEN];
inet_ntop(AF_INET, if_addr, addressBuffer, INET_ADDRSTRLEN);

char hostname[1024];
hostname[1023] = '\0';
gethostname(hostname, 1023);

strcpy(databuf, "My IP is " );
strcat(databuf, addressBuffer);
strcat(databuf, " and my hostname is ");
strcat(databuf, hostname);

 if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);

printf("Sending: %s\n", databuf);

if(sendto(sd, databuf, datalen, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0)
{perror("Sending datagram message error");}
else
  printf("Sending datagram message...OK\n");
 
/* Try the re-read from the socket if the loopback is not disable
if(read(sd, databuf, datalen) < 0)
{
perror("Reading datagram message error\n");
close(sd);
exit(1);
}
else
{
printf("Reading datagram message from client...OK\n");
printf("The message is: %s\n", databuf);
}
*/
return 0;
}
