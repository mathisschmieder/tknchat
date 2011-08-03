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

#define DEBUG

int debug = 0;

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
int masterdelay;
int maxreq;
int seqno;
packet request;

ClientCredentials MyClientCredentials;
BrowseList MyBrowseList;
sockaddr_in * getIP(const char*);


int main(int argc, char** argv) {
  parse_options(argc, argv);
  
  srand( time(NULL) ); //maybe take a better seed
  OS_Level = rand() % 65535 + 1;

  #ifdef DEBUG
  printf("DEBUG OS_Level %d\n", OS_Level);
  #endif

  if (eth == NULL) 
    eth = "eth0"; 

  MyClientCredentials.sockaddr = getIP(eth);
  MyClientCredentials.name[1023] = '\0';
  if (nick == NULL)
    gethostname(MyClientCredentials.name, 1023);
  else
    strncpy(MyClientCredentials.name, nick, sizeof(MyClientCredentials.name));

  MyBrowseList.head = NULL;

  sd = setup_multicast();
  init_fdSet(&rfds);
  
  send_multicast(SEARCHING_MASTER, NULL);

  setNewState(STATE_INIT);

  globalTimer.tv_sec = 1;
  globalTimer.tv_usec = 0;

  for (;;) { // main loop
    
    retval = select(sizeof(&rfds)*8, &rfds, NULL, NULL, (struct timeval*)&globalTimer);
    assert(retval >= 0);

    if (retval == 0) {
      // STATE MACHINE
      switch(appl_state) {
        
        case STATE_INIT:
          pdebug("STATE_INIT");
          setNewState(STATE_FORCE_ELECTION);
          break;
        
        case STATE_MASTER_FOUND:
          pdebug("STATE_MASTER_FOUND");
          if (maxreq > 0) {
            send_multicast(GET_BROWSE_LIST, NULL);
            maxreq--;
          } else {
            send_multicast(FORCE_ELECTION, NULL);
            setNewState(STATE_FORCE_ELECTION);
          }
          break;

        case STATE_BROWSELIST_RCVD:
          pdebug("STATE_BROWSELIST_RCVD");
          //setup_unicast();
          break;

        case STATE_FORCE_ELECTION:
          pdebug("STATE_FORCE_ELECTION");

          char char_OS_Level[sizeof(OS_Level)*8+1];
          sprintf(char_OS_Level, "%d", htonl(OS_Level));
          send_multicast(MASTER_LEVEL, char_OS_Level);

          setNewState(STATE_I_AM_MASTER);
          masterdelay = 4; // wait 3 cycles until we are sure that we are the master
          break;

        case STATE_I_AM_MASTER:
          pdebug("STATE_I_AM_MASTER");
          if (masterdelay > 1)
            masterdelay--;
          else if (masterdelay == 1) {
            mc_packet request;
            request.type = MC_GET_CLIENTCREDENTIALS;
            masterdelay = 0;
          }
          break;
      }
    } else {

      if (FD_ISSET(sd, &rfds)) {
        packet mc_recv;
        recv(sd, &mc_recv, sizeof(mc_recv), 0);
        local_packet mc_packet;
        mc_packet = receive_packet(mc_recv);
       /* if ((appl_state == STATE_INIT) || (appl_state == STATE_FORCE_ELECTION) && (mc_recv.type = MC_I_AM_MASTER)) {
          pdebug("Master found");
          maxreq = 5; 
          setNewState(STATE_MASTER_FOUND);
        }
        else if ((appl_state == STATE_I_AM_MASTER) && (mc_recv.type == MC_OS_LEVEL) && (mc_recv.OS_Level > OS_Level)) {
          pdebug("Master found");
          maxreq = 5;
          setNewState(STATE_MASTER_FOUND);
        }
        else if ((appl_state == STATE_I_AM_MASTER) && (mc_recv.type == MC_REQUEST_MEMBERSHIP)) {
          pdebug("Sending I_AM_MASTER");
          send_multicast(I_AM_MASTER, NULL);
        } else if (mc_recv.type == MC_FORCE_ELECTION) {
            char char_OS_Level[sizeof(OS_Level)*8+1];
            sprintf(char_OS_Level, "%d", htonl(OS_Level));
            send_multicast(MASTER_LEVEL, char_OS_Level);
            setNewState(STATE_FORCE_ELECTION);
        }
*/
      }

    } 
    init_fdSet(&rfds);
    setGlobalTimer(1,0);
  }


  close(sd);
  return 0;
}

void pdebug(const char* message)
{
  #ifdef DEBUG
  printf("DEBUG %s\n", message);
  #endif
}

void version() 
{
  // TODO: git version?
  printf("tknchat v0.1\n");
}

void usage()
{
  printf("usage:\n");
  printf(" -h --help\t print this help\n");
  printf(" -v --version\t show version\n");
  printf(" -i --interface\t set primary interface (default eth0)\n");
  printf(" -n --nick\t set nickname (default Hostname)\n");
}

void parse_options(int argc, char** argv) {
  
  pdebug("DEBUG");
  int ret;
  
  while ((ret = getopt_long(argc,argv,"hvi:n:",
			    long_options, &option_index)) != EOF)
    switch (ret) {
    case 'h':
      version();
      usage();
      exit(0);
      break;               
    case 'v':
      version();
      exit(0);
      break;               
    case 'i':
      eth = optarg;
      break;               
    case 'n':
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

  pdebug("multicast set up");
  return sd;
}

int send_multicast(int type, char* data) {
  packet packet;
  packet = create_packet(type, data);
  if ( data != NULL)
    //                                    +4 (header) + 4 (magic 4 data bytes.. TODO!)
    return sendto(sd, (char *)&packet, sizeof(data) + 8, 0, (struct sockaddr*)&msock, sizeof(msock));
  else
    return sendto(sd, (char *)&packet, 4, 0, (struct sockaddr*)&msock, sizeof(msock));
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

packet create_packet(int type, char* data) {
  seqno++; //increment sequence number by one
 
  #ifdef DEBUG
  printf("DEBUG creating packet %d\n", seqno);
  printf("DEBUG packet type: %d\n", type);
  #endif

  packet packet;
  uint32_t header;
  uint16_t datalen;

  if (data != NULL) { //we dont need neither datalen nor data if there is no data
    datalen = sizeof(data);
    //                              TODO: magic 4 data bytes
    strncpy(packet.data, data, sizeof(data)+4);
  } else
    datalen = 0;
  //        Headerformat
  //        version 2   type 5        options 1 seqno 8      datalen 16 = 32 bit
  header = (0x01 << 30)|(type << 25)|(0 << 24)|(seqno << 16)|(datalen);
  packet.header = htonl(header);
  return packet;
}

local_packet receive_packet(packet packet) {
  local_packet local_packet;
  uint32_t header;

  header = ntohl(packet.header);
  local_packet.version = (header >> 30) & 3;
  local_packet.type = (header >> 25) & 31;
  local_packet.options = (header >> 24) & 1;
  local_packet.seqno = (header >> 16) & 255;
  local_packet.datalen = header & 65535;

  printf("received type: %d\n", local_packet.type);

  strncpy(local_packet.data, packet.data, local_packet.datalen);

  return local_packet;
}
