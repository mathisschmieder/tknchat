/* Receiver/client multicast Datagram example. */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
 
#define MC_GROUP_ADDR "225.2.0.1"
#define MC_GROUP_PORT 5020

#define S0READY 0x00
#define S1READY 0X01
#define S2READY 0X02

int sd, so;
struct sockaddr_in localSock;
struct ip_mreq group;

int datalen;
char databuf[1024];
 
int main(int argc, char *argv[])
{
/* SOCKET */
  printf("SOCKET\n");
  /* Create a socket on which to receive. */
  so = socket(AF_INET, SOCK_STREAM, 0);
  if(so < 0)
  {
  perror("Opening socket error");
  exit(1);
  }
  else
  printf("Opening socket....OK.\n");

/* DATAGRAM PART SOCKET */
  printf("DATAGRAM SOCKET\n");
  /* Create a datagram socket on which to receive. */
  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sd < 0)
  {
  perror("Opening datagram socket error");
  exit(1);
  }
  else
  printf("Opening datagram socket....OK.\n");
 
  /* Enable SO_REUSEADDR to allow multiple instances of this */
  /* application to receive copies of the multicast datagrams. */
  int reuse = 1;
  if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
  {
    perror("Setting SO_REUSEADDR error");
    exit(1);
  }
  else
  printf("Setting SO_REUSEADDR...OK.\n");

   
  /* Bind to the proper port number with the IP address */
  /* specified as INADDR_ANY. */
  memset((char *) &localSock, 0, sizeof(localSock));
  localSock.sin_family = AF_INET;
  localSock.sin_port = htons(MC_GROUP_PORT);
  localSock.sin_addr.s_addr = INADDR_ANY;

  if(bind(sd, (struct sockaddr*)&localSock, sizeof(localSock)))
  {
  perror("Binding datagram socket error");
  //close(sd);
  exit(1);
  }
  else
  printf("Binding datagram socket...OK.\n");

   
  /* Join the multicast group 226.1.1.1 on the local 203.106.93.94 */
  /* interface. Note that this IP_ADD_MEMBERSHIP option must be */
  /* called for each local interface over which the multicast */
  /* datagrams are to be received. */
  group.imr_multiaddr.s_addr = inet_addr(MC_GROUP_ADDR);
  group.imr_interface.s_addr = INADDR_ANY;
  if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
  {
  perror("Adding multicast group error");
  //close(sd);
  exit(1);
  }
  else
  printf("Adding multicast group...OK.\n");


/* SELECT */
printf("SELECT\n");
  struct timeval tv;
  int retval;
  fd_set rfds;




  int count = 0;
  while ( count < 2 )
  { 
    FD_ZERO(&rfds);
    /* Watch stdin (fd 0) to see when it has input. */
    FD_SET(0, &rfds);
    //  FD_SET(so, &rfds);
    FD_SET(sd, &rfds);

    /* Wait up to five seconds. */
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    printf("while %d\n",retval);
    retval = select(sizeof(rfds)*8, &rfds, NULL, NULL, &tv);
    /* Don't rely on the value of tv now! */

   if (retval == -1)
        perror("select()");
    else if (retval == 0)
    {
      printf("No data within five seconds.\n");
    }
    else if (retval > 0)
    {
      // TODO: stdin as a socket?
      if (FD_ISSET(0, &rfds))
      {  
        datalen = sizeof(databuf);
        if(fgets (databuf, datalen, stdin) < 0)
        {
          perror("Reading message error");
          // TODO: close sockets
          //close(sd); 
          exit(1);
        }
        else
        {
          printf("stdin message: \"%s\"\n", databuf);
        }
      }
      if (FD_ISSET(sd, &rfds)) 
      {
        printf("ready: %d\n", FD_ISSET(sd, &rfds));
        datalen = sizeof(databuf);
        if(recv(sd, databuf, datalen, 0) < 0)
        {
          perror("Reading message error");
          //close(sd); 
          exit(1);
        }
        else
        {
          printf("multicast message: \"%s\"\n", databuf);
        }
      }
    }
    //count++;
  }

  return 0;
}
