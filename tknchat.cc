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

// argument parse 
int option_index = 0;
const char* eth = NULL; 
const char* nick = NULL; 

fd_set rfds;

int appl_state = STATE_NULL;;
struct timeval globalTimer;

char buf[MAX_MSG_LEN];
int buflen;
int OS_Level = 0;
int retval;
int masterdelay;
int maxreq;
int seqno;
int browselistlength = 0;
char host[1024];
in_addr localip;

int main(int argc, char** argv) {
  parse_options(argc, argv);
  
  srand( time(NULL) ); //maybe take a better seed
  OS_Level = rand() % 65535 + 1;

  #ifdef DEBUG
  printf("DEBUG OS_Level %d\n", OS_Level);
  #endif

  host[1023] = '\0';
  gethostname(host, 1023);

  if (eth == NULL) 
    eth = "eth0"; 

  localip = getIP(eth);

  sd = setup_multicast();
  init_fdSet(&rfds);
  
  globalTimer.tv_sec = 1;
  globalTimer.tv_usec = 0;

  maxreq = 5;

  #ifdef IAMMASTER
    send_multicast(I_AM_MASTER, NULL);
    exit(0);
  #endif

  #ifdef BROWSELIST
    send_multicast(BROWSE_LIST, NULL);
    exit(0);
  #endif

  local_packet mc_packet;
  for (;;) { // main loop
    
    // READ FROM SOCKETS 
    retval = select(sizeof(&rfds)*8, &rfds, NULL, NULL, (struct timeval*)&globalTimer);
    assert(retval >= 0);

    if ((retval > 0) && (FD_ISSET(sd, &rfds))) {
      packet mc_recv;
      memset(mc_recv.data, 0, strlen(mc_recv.data));
      recv(sd, &mc_recv, sizeof(mc_recv), 0);

      mc_packet = receive_packet(mc_recv);
    } else {
      mc_packet.type = (int)NULL;
      memset(mc_packet.data, 0, strlen(mc_packet.data));
    }


    // STATE MACHINE
    switch(appl_state) {

      case STATE_NULL:
        pdebug("STATE_NULL");
        // a: send_SEARCHING_MASTER
        send_multicast(SEARCHING_MASTER, NULL);
        setNewState(STATE_INIT);
        break;
      
      case STATE_INIT:
        pdebug("STATE_INIT");
        if ( maxreq > 0) {
          maxreq--;
          // e: rcvd_I_am_Master
          // a: send_get_browse_list
          if (mc_packet.type == I_AM_MASTER) {
            pdebug("master found");
            maxreq = 5;
            setNewState(STATE_MASTER_FOUND);
          } else if (mc_packet.type == FORCE_ELECTION) {
            setNewState(STATE_FORCE_ELECTION);
            break;
          } else
            send_multicast(SEARCHING_MASTER, NULL);
        }
        // e: Timeout
        // a: send_force_election
        else {
          send_multicast(FORCE_ELECTION, NULL);
          setNewState(STATE_FORCE_ELECTION);
        }

        break;
      
      case STATE_MASTER_FOUND:
        pdebug("STATE_MASTER_FOUND");

        reset_browselist(); //reset browse list

        // e: rcvd_browse_list 
        // a: manage_member_list 
        // a: establishConn
        if (mc_packet.type == BROWSE_LIST) {
          maxreq = 5;

          BrowseListItem client;
          strncpy((char*)&client, mc_packet.data, strlen(mc_packet.data)); //copy received data into BrowseListItem struct

          printf("DEBUG: received browse list item %s\n", client.ip);
          printf("test data: %s\n", (char*)&client);

          setNewState(STATE_BROWSELIST_RCVD);
        } else {
          // e: Timeout && #req < MAXREQ 
          // a: send_get_browse_list
          if (maxreq > 0) {
            maxreq--;
            send_multicast(GET_BROWSE_LIST, NULL);
            // e: Timeout && #req > MAXREQ 
            // a: send_force_election
          } else {
            maxreq = 5;
            send_multicast(FORCE_ELECTION, NULL);
            setNewState(STATE_FORCE_ELECTION);
          }
        }
        break;

      case STATE_BROWSELIST_RCVD:
        pdebug("STATE_BROWSELIST_RCVD");
        // TODO: nice way of handling master dropouts
        // Trying to periodically refresh browselist 
        // and see if master is still there
        // TODO:
        // setup_unicast();

        // e: rcvd_browse_list
        // e: rcvd_leaved
        // a: manage_member_list
        if ((mc_packet.type == BROWSE_LIST) || (mc_packet.type == LEAVE_GROUP)) {
          // TODO: manage_member_list
        }
        break;

      case STATE_FORCE_ELECTION:
        pdebug("STATE_FORCE_ELECTION");
        // e: rcvd_master_level
        // a: Am_I_the_Master? No
        if ((mc_packet.type == I_AM_MASTER) || 
            ( (mc_packet.type == MASTER_LEVEL) && (ntohl(atoi(mc_packet.data)) > OS_Level) )) {
          //TODO: better handling of waiting time until the new master is ready
          maxreq = 5;
          setNewState(STATE_MASTER_FOUND);
          break;
        }
        // e: Timeout or rcvd_master_level < mine 
        // a: send_I_am_Master 
        // a: send_get_member_info
        else {
          char char_OS_Level[sizeof(OS_Level)*8+1];
          sprintf(char_OS_Level, "%d", htonl(OS_Level));
          send_multicast(MASTER_LEVEL, char_OS_Level);
          pdebug("assuming I am master");
          setNewState(STATE_I_AM_MASTER);
          masterdelay = 4; // wait 3 cycles until we are sure that we are the master
        }
        break;

      case STATE_I_AM_MASTER:
        pdebug("STATE_I_AM_MASTER");
        if ( mc_packet.type == (int)NULL ) { //have we received a multicast packet?
          if (masterdelay > 1)
            masterdelay--;
          else if (masterdelay == 1) {
            masterdelay = 0; // we now are sure that we are the master
            send_multicast(GET_MEMBER_INFO,NULL); //therefore we ask all clients to send their credentials
          }       
        } else {
            // e: rcvd_master_level greater than mine
            // a: am_I_the_Master? No
          if ((mc_packet.type == MASTER_LEVEL) && (ntohl(atoi(mc_packet.data)) > OS_Level)) {
            maxreq = 5;
           //TODO: better handling of waiting time until the new master is ready 
            setNewState(STATE_MASTER_FOUND);
            break;
          }

          // e: rcvd_get_browselist
          // a: send_browselist
          else if (mc_packet.type == GET_BROWSE_LIST) {

            BrowseListItem test;
            strncpy(test.ip, "127.000.000.001", INET_ADDRSTRLEN);

            printf("test: %s\n", (char*)&test);
            send_multicast(BROWSE_LIST, (char*)&test);
          } 

          // e: rcvd_set_member_info
          // a: manage_member_list
          else if (mc_packet.type == SET_MEMBER_INFO) {
            // TODO: manage_member_list
            //  pdebug("requesting member info");
            //  browselistlength = 0;
            //  addToBrowseList(inet_ntoa(localip), browselistlength);
            //  send_multicast(GET_MEMBER_INFO, NULL); // request every client's credentials
          } 
          // e: rcvd_searching_master
          // a: send_I_am_Master
          else if ( mc_packet.type == SEARCHING_MASTER ) {
            send_multicast(I_AM_MASTER, NULL);
          }
        }
        break;
    }
    pdebug("init fdset");
    init_fdSet(&rfds);
    setGlobalTimer(1,0);
  }

   // else {

   //   if (FD_ISSET(sd, &rfds)) {
   //     packet mc_recv;
   //     memset(mc_recv.data, 0, strlen(mc_recv.data));
   //     recv(sd, &mc_recv, sizeof(mc_recv), 0);
   //     local_packet mc_packet;

   //     mc_packet = receive_packet(mc_recv);
   //     if ((appl_state == STATE_INIT) || (appl_state == STATE_FORCE_ELECTION) && (mc_packet.type == I_AM_MASTER)) {
   //       pdebug("Master found");
   //       maxreq = 5; 
   //       setNewState(STATE_MASTER_FOUND);
   //     }
   //     else if ((appl_state == STATE_I_AM_MASTER) && (mc_packet.type == MASTER_LEVEL) 
   //         && (ntohl(atoi(mc_packet.data)) > OS_Level)) {
   //       pdebug("Master found");
   //       maxreq = 5;
   //       setNewState(STATE_MASTER_FOUND);
   //     }
   //     else if ((appl_state == STATE_I_AM_MASTER) && (mc_packet.type == SEARCHING_MASTER)) {
   //       pdebug("Sending I_AM_MASTER");
   //       send_multicast(I_AM_MASTER, NULL);
   //       send_multicast(GET_MEMBER_INFO, NULL);
   //     } else if ((appl_state == STATE_I_AM_MASTER) && (mc_packet.type == GET_BROWSE_LIST)) {
   //       pdebug("Sending BrowseList");
   //       send_multicast(BROWSE_LIST, NULL); //TODO: send actual browse list
   //     } else if (mc_packet.type == FORCE_ELECTION) {
   //       char char_OS_Level[sizeof(OS_Level)*8+1];
   //       sprintf(char_OS_Level, "%d", htonl(OS_Level));
   //       send_multicast(MASTER_LEVEL, char_OS_Level);
   //       masterdelay = 4; // wait 3 cycles until we are sure that we are the master
   //       setNewState(STATE_I_AM_MASTER);
   //     } else if ((appl_state == STATE_I_AM_MASTER) && (mc_packet.type == SET_MEMBER_INFO)) {
   //       printf("got client credentials: %s\n", mc_packet.data);
   //       addToBrowseList(mc_packet.data, browselistlength);
   //     } else if ((appl_state == STATE_MASTER_FOUND) && (mc_packet.type == GET_MEMBER_INFO)) {
   //       pdebug("Sending MEMBER_INFO");
   //       send_multicast(SET_MEMBER_INFO, inet_ntoa(localip));
   //     } else if ((appl_state == STATE_MASTER_FOUND) && (mc_packet.type == BROWSE_LIST)) {
   //       // new
   //       pdebug("received updated browselist");
   //       maxreq = 5;
   //       setNewState(STATE_BROWSELIST_RCVD);
   //     }
   //   }
   // } 

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
in_addr getIP(const char* eth) {
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

  return sockaddr->sin_addr;
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
  if ( data != NULL) {
    //                                    +4 (header) 
    return sendto(sd, (char *)&packet, strlen(data) + 4, 0, (struct sockaddr*)&msock, sizeof(msock));
  }
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
  #ifdef DEBUG
    printf("sec: %d, usec: %d\n", sec, usec);
  #endif
  globalTimer.tv_sec  = sec;
  globalTimer.tv_usec = usec;
  
// TODO: WTF
//  static int i_sec;
//  static int i_usec;
//  if ((globalTimer.tv_sec == -1) && (globalTimer.tv_usec == -1))
//    {
//      printf("1\n");
//      globalTimer.tv_sec  = i_sec;
//      globalTimer.tv_usec = i_usec;
//      return;
//    }
//
//  i_sec  = sec;
//  i_usec = usec;
//
// TODO: DELUXE BRACKET WTF
//  if (sec <= globalTimer.tv_sec)
//    if (usec <= globalTimer.tv_usec)
//       printf("2\n");
//  {
//	globalTimer.tv_sec  = sec;
//	globalTimer.tv_usec = usec;
//      }
//
//  if ((globalTimer.tv_sec == 0) && (globalTimer.tv_usec == 0)) {
//    printf("3\n");
//    globalTimer.tv_sec  = sec;
//    globalTimer.tv_usec = usec;
//   }
}

packet create_packet(int type, char* data) {
  seqno++; //increment sequence number by one
 
  #ifdef DEBUG
  printf("DEBUG creating packet with seq no %d\n", seqno);
  printf("DEBUG creating packet type: %d\n", type);
  #endif

  packet packet;
  uint32_t header;
  uint16_t datalen;

  memset(packet.data, 0, strlen(packet.data));

  if (data != NULL) { //we dont need neither datalen nor data if there is no data
    datalen = strlen(data);
    strncpy(packet.data, data, datalen);
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

  memset(local_packet.data, 0, strlen(local_packet.data));
#ifdef DEBUG
  printf("DEBUG received type: %d\n", local_packet.type);
#endif

  if (local_packet.datalen != 0) {
    strncpy(local_packet.data, packet.data, sizeof(packet.data));
  }

  return local_packet;
}

void addToBrowseList(char* clientip, int i) {
  pdebug("adding item to browse list");
  strncpy(browselist[i].ip, clientip, INET_ADDRSTRLEN);

  hostent* host;
  in_addr ip;
  ip.s_addr = inet_addr(clientip);
  host = gethostbyaddr((char*)&ip, sizeof(ip), AF_INET);
  strncpy(browselist[i].name, host->h_name, strlen(host->h_name));

#ifdef DEBUG
  printf("ip: %s\n", browselist[i].ip);
  printf("host: %s\n", browselist[i].name);
#endif
}

void reset_browselist() {
  for (int i = 0; i < MAX_MEMBERS - 1; i++) {
    memset(browselist[i].name, 0, strlen(browselist[i].name));
    memset(browselist[i].ip, 0, INET_ADDRSTRLEN);
  }
  browselistlength = 0;
}
