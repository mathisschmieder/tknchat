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

// Options for argument parsing
int option_index = 0;
const char* eth = NULL; 
const char* nick = NULL; 

// Define our file descriptor set
fd_set rfds;

// State machine
int appl_state = STATE_NULL;;
struct timeval globalTimer;
int OS_Level = 0;
int maxreq;
int alive_req;
int masterdelay;

// Communication
int retval;
char buf[MAX_MSG_LEN];
int buflen;
int seqno;
int browselistlength = 0;
char host[1024];
in_addr localip;
int localindex;

// The main method
int main(int argc, char** argv) {
  // Parse arguments from the command line
  parse_options(argc, argv);


  // TODO
  // maybe take a better seed
  srand( time(NULL) ); 
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
  s = setup_unicast_listen();
  init_fdSet(&rfds);

  
  globalTimer.tv_sec = 1;
  globalTimer.tv_usec = 0;

  maxreq = 5;

  // These options produce different binaries for debugging
  #ifdef IAMMASTER
    send_multicast(I_AM_MASTER, NULL);
    exit(0);
  #endif

  #ifdef BROWSELIST
    send_multicast(BROWSE_LIST, NULL);
    exit(0);
  #endif

  // Multicast packet
  local_packet mc_packet;
  for (;;) { // main loop
    
    // READ FROM SOCKETS 
    // select monitors the file descriptors in fdset and returns a positive integer if
    // there is data available
    retval = select(sizeof(&rfds)*8, &rfds, NULL, NULL, (struct timeval*)&globalTimer);
    // Abort execution if select terminated with an error
    assert(retval >= 0);

    //clearing last states multicast packet
    mc_packet.type = (int)NULL;
    memset(mc_packet.data, 0, strlen(mc_packet.data));


    // Get data from sockets 
    if (retval > 0) {
      if (FD_ISSET(sd, &rfds)) {
        packet mc_recv;
        memset(mc_recv.data, 0, strlen(mc_recv.data));
        recv(sd, &mc_recv, sizeof(mc_recv), 0);

        mc_packet = receive_packet(mc_recv);
      } else if (FD_ISSET(s, &rfds)) {
        //we have a new visitor aka data on unicast listening socket
        int newsock, cli_len, valid;
        struct sockaddr_in client;
        cli_len = sizeof(client);
        valid = 0;
        newsock = accept(s, (sockaddr *)&client, (socklen_t *)&cli_len);

#ifdef DEBUG
        printf("DEBUG new incoming unicast connection from %s\n", inet_ntoa(client.sin_addr));
#endif
      
        //find out where the connection came from
        for (int i = 0; i < MAX_MEMBERS; i++) {
          if (strncmp(inet_ntoa(client.sin_addr), browselist[i].ip, INET_ADDRSTRLEN) == 0 ) {
            browselist[i].socket = newsock;
            valid = 1;
            break;
          }
        }
        if (valid == 0) {
          pdebug("closing non-authorized unicast connection");
          close(newsock);
        }
      } else if (FD_ISSET(0, &rfds)) {
        // keyboard input
        char buffer[MAX_MSG_LEN];
        char * p;

        memset(buffer, 0, MAX_MSG_LEN); //clear buffer
        fgets(buffer, MAX_MSG_LEN, stdin);     
        
        //remove trailing newline character                                       
        if ((p = strchr(buffer, '\n')) != NULL)
          *p = '\0';

        if (strncmp("/", &buffer[0], 1) == 0) { // input with / is a command
          if (strncmp("quit", &buffer[1], 5) == 0)
            close_chat();
        }

        send_unicast(DATA_PKT,buffer);
      }
      for (int i = 0; i < MAX_MEMBERS; i++) {
        if (browselist[i].socket != 0) {
          if (FD_ISSET(browselist[i].socket, &rfds)) {
            #ifdef DEBUG
              printf("receiving data\n");
            #endif
            local_packet uc_packet;
            packet uc_recv;
            memset(uc_recv.data, 0, strlen(uc_recv.data));
            recv(browselist[i].socket, &uc_recv, sizeof(uc_recv), 0);

            uc_packet = receive_packet(uc_recv);
            printf("unicast received type: %d\n", uc_packet.type);
            printf("unicast received from: %s\n", browselist[i].name);

            if ((uc_packet.type == DATA_PKT) && ((int)strlen(uc_packet.data) > 0)) {
              printf("%s>> %s\n", browselist[i].name, uc_packet.data);
            } else //only null-data packet on unicast is LEAVE_GROUP 
              removeFromBrowseList(i);
          }
        }
      }
    } 

    // STATE MACHINE
    switch(appl_state) {

      case STATE_NULL:
        pdebug("STATE_NULL");
        // a: send_SEARCHING_MASTER
        send_multicast(SEARCHING_MASTER, inet_ntoa(localip));
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
            maxreq = 6;
            // reset browse list
            reset_browselist(); 

            setNewState(STATE_MASTER_FOUND);
          } 
          // e: rcvd_force_election
          else if (mc_packet.type == FORCE_ELECTION) {
            setNewState(STATE_FORCE_ELECTION);
            break;
          } 
          else {
            send_multicast(SEARCHING_MASTER, inet_ntoa(localip));
          }
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
        // e: rcvd_browse_list 
        // a: manage_member_list 
        // a: establishConn
        if (mc_packet.type == BROWSE_LIST) {
          maxreq = 5;
          receive_BrowseListItem(mc_packet.data);
          setNewState(STATE_BROWSELIST_RCVD);
        } 
        else if (mc_packet.type == GET_MEMBER_INFO) {
          maxreq = 6;
          pdebug("SENDING MEMBER INFO");
          send_multicast(SET_MEMBER_INFO, inet_ntoa(localip));
        }
        else if (mc_packet.type == (int)NULL ) { //only decrement maxreq if there is no received packet
          // e: Timeout && #req < MAXREQ 
          // a: send_get_browse_list
          if (maxreq == 6) {
            maxreq--;
            send_multicast(GET_BROWSE_LIST, NULL);
          }
          else if (maxreq > 0) {
            maxreq--;

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
        // TODO nice way of handling master dropouts
        // Trying to periodically refresh browselist 
        // and see if master is still there

        if ( mc_packet.type == (int)NULL ) { //if there is no data, increment alive_req to check if the master still lives
          if (alive_req < 25) {
            alive_req++;
          } else {
            pdebug("master dead!");
            setNewState(STATE_FORCE_ELECTION);
          }
        } else if ((mc_packet.type == CTRL_PKT) && (mc_packet.datalen == 0)) { //master ping, respond to it with a control packet and our ip
          alive_req = 0;
          pdebug("master alive");
          send_multicast(CTRL_PKT, inet_ntoa(localip));
        } else if (mc_packet.type == GET_MEMBER_INFO) { //master requests our info. supply them and go into STATE_MASTER_FOUND to wait for new browselist
          maxreq = 6; //possible FIXME for segfaults
          pdebug("SENDING MEMBER INFO");
          send_multicast(SET_MEMBER_INFO, inet_ntoa(localip));
          //TODO better handling of waiting time until the new master is ready <- this still necessary? 
          reset_browselist();
          setNewState(STATE_MASTER_FOUND);
        } else if (mc_packet.type == LEAVE_GROUP_MASTER) {
          maxreq = 5;
          send_multicast(STATE_FORCE_ELECTION, NULL);
          setNewState(STATE_FORCE_ELECTION);
        }
        // e: rcvd_browse_list
        // e: rcvd_leaved
        // a: manage_member_list
        else if ((mc_packet.type == BROWSE_LIST)) {
          reset_browselist();
          receive_BrowseListItem(mc_packet.data);
          // TODO differs from state tree diagram
          // should stay in this state
          // mathis: true, but it does break stuff!
          // maxreq = 5;
          // setNewState(STATE_MASTER_FOUND);
        }
        if ( setup_unicast() < 0) {
          pdebug("error setting up unicast connections, requesting new browse list");
          reset_browselist();
          maxreq = 6;
          setNewState(STATE_MASTER_FOUND);
        }
        break;

      case STATE_FORCE_ELECTION:
        pdebug("STATE_FORCE_ELECTION");
        // e: rcvd_master_level > mine
        // a: Am_I_the_Master? No
        if ((mc_packet.type == I_AM_MASTER) || 
            ( (mc_packet.type == MASTER_LEVEL) && (ntohl(atoi(mc_packet.data)) > OS_Level) )) {
          //TODO better handling of waiting time until the new master is ready
          maxreq = 5;
          // reset browse list
          reset_browselist(); 

          setNewState(STATE_MASTER_FOUND);
          break;
        }
        // e: Timeout or rcvd_master_level < mine 
        // a: send_I_am_Master 
        // a: send_get_member_info
        else {
          // Create char to hold the OS_LEVEL 
          char char_OS_Level[sizeof(OS_Level)*8+1];
          sprintf(char_OS_Level, "%d", htonl(OS_Level));
          // And send it via multicast
          send_multicast(MASTER_LEVEL, char_OS_Level);
          pdebug("assuming I am master");
          maxreq = 5;
          setNewState(STATE_I_AM_MASTER);
          // Wait 3 cycles until we are sure that we are the master
          masterdelay = 4; 
        }
        break;

      case STATE_I_AM_MASTER:
        pdebug("STATE_I_AM_MASTER");
        // Have we received a multicast packet?
        if ( mc_packet.type == (int)NULL ) { 
          if (masterdelay > 1)
            masterdelay--;
          else if (masterdelay == 1) {
            // Now we are the master
            masterdelay = 0; 
            // Initialize BrowseList
            reset_browselist();
            addToBrowseList(inet_ntoa(localip), browselistlength++);
            // Ask all clients for their credentials
            //send_multicast(BROWSE_LIST, 
            send_multicast(GET_MEMBER_INFO,NULL); 
          }       
        } else {
            // e: rcvd_master_level greater than mine
            // a: am_I_the_Master? No
          if ((mc_packet.type == MASTER_LEVEL) && (ntohl(atoi(mc_packet.data)) > OS_Level)) {
            maxreq = 5;
            //TODO better handling of waiting time until the new master is ready 
            reset_browselist();
            setNewState(STATE_MASTER_FOUND);
            break;
          }
          // e: rcvd_get_browselist
          // a: send_browselist
          else if (mc_packet.type == GET_BROWSE_LIST) {
            for (int i = 0; i < browselistlength; i++) {
            #ifdef DEBUG
              printf("DEBUG sending browselistentry %d\n", i);
            #endif
              send_BrowseListItem(i);
            }
          } 
          // e: rcvd_set_member_info
          // a: manage_member_list
          else if (mc_packet.type == SET_MEMBER_INFO) {
            // manage_member_list
            addToBrowseList(mc_packet.data, browselistlength++);
          } 
          // e: rcvd_searching_master
          // a: send_I_am_Master
          else if ( mc_packet.type == SEARCHING_MASTER ) {
            send_multicast(I_AM_MASTER, NULL);
            addToBrowseList(mc_packet.data, browselistlength++);
          } else if ( mc_packet.type == FORCE_ELECTION ) {
            setNewState(STATE_FORCE_ELECTION);
          } else if ( mc_packet.type == LEAVE_GROUP ) {
            pdebug("slave quit");
            alive_req = 0;
            maxreq = 10;
            masterdelay = 1;
          }
        }

        if (maxreq > 0) {
          maxreq--;
          if (alive_req > 1) {
            if ((mc_packet.type == CTRL_PKT) && (mc_packet.datalen > 0)) {
              alive_req--;
            }
          }
          // All slaves replied
          else if (alive_req == 1) {
            alive_req = 0;
            pdebug("all slaves alive");
          }
        }
        else {
          if (alive_req > 0) {
            pdebug("slave dead!");
            // TODO best way to handle this?
            alive_req = 0;
            maxreq = 10;
            masterdelay = 1;
          }
          else {
            maxreq = 10;
            alive_req = browselistlength;
            pdebug("master ping!");
            send_multicast(CTRL_PKT, NULL);
          }
        }
        break;
    }
    init_fdSet(&rfds);
    setGlobalTimer(1,0);
  }
  
  close_chat();
  return 0;
}

// Function to output debug messages
// when the DEBUG option is set
void pdebug(const char* message)
{
  #ifdef DEBUG
  printf("DEBUG %s\n", message);
  #endif
}

// Function to output program version
void version() 
{
  // TODO git version?
  printf("tknchat v0.3\n");
}

// Function for basic help
void usage()
{
  printf("usage:\n");
  printf(" -h --help\t print this help\n");
  printf(" -v --version\t show version\n");
  printf(" -i --interface\t set primary interface (default eth0)\n");
  //TODO remove nickname
  printf(" -n --nick\t set nickname (default Hostname)\n");
}

// Function for parsing and handling command line arguments 
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

// Function to get own IP adress
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

// Function to initialize the file descriptor set
int init_fdSet(fd_set* fds) {
  // reset fds
  FD_ZERO(fds);
  // add console 
  FD_SET(0, fds);
  // add multicast 
  FD_SET(sd, fds);
  // add unicast incoming port
  FD_SET(s, fds);
  // add unicast connections
  for (int i = 0; i < MAX_MEMBERS; i++) 
    if (browselist[i].socket != 0) 
      FD_SET(browselist[i].socket, fds);
}

// Function to setup multicast communication
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

int setup_unicast_listen() {
  int s;
  struct sockaddr_in srv;

  pdebug("setting up incoming socket");

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("Error opening local listening socket");
    exit(1);
  }

  srv.sin_addr.s_addr = INADDR_ANY;
  srv.sin_port = htons(UC_DATA_PORT);
  srv.sin_family = AF_INET;
  if (bind(s, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
    perror("Error binding local listening socket");
    exit(1);
  }

  if (listen(s,5) < 0) {
    perror("Error listening to local socket");
    exit(1);
  }

  return s;
}

int setup_unicast() {
  int newsock;
  struct sockaddr_in options;
  for (int i = 0; i < localindex; i++) {
    if ((browselist[i].socket == 0) 
        && (strncmp(inet_ntoa(localip), browselist[i].ip, INET_ADDRSTRLEN) != 0 ) // this should be obsolete
        && (strncmp(browselist[i].ip, "", INET_ADDRSTRLEN) != 0)) {               // so should this
      #ifdef DEBUG
        printf("DEBUG opening connection to %s, i is %d\n", browselist[i].ip, i);
      #endif
      newsock = socket(AF_INET, SOCK_STREAM, 0);
      if (newsock < 0) {
        perror("Error opening unicast socket, closing chat");
        close_chat();
      }

      options.sin_addr.s_addr = inet_addr(browselist[i].ip);
      // TODO URGENT
      // is it even possible for several clients to connect on the same port?
      options.sin_port = htons(UC_DATA_PORT);
      options.sin_family = AF_INET;
      if (connect(newsock, (struct sockaddr *)&options, sizeof(options)) < 0) {
        perror("Error connecting to unicast socket");
        return -1;
      }

      browselist[i].socket = newsock;
    } 
  }
  return 0; //TODO
}

// Function to send a multicast packet
int send_multicast(int type, char* data) {
  packet packet;
  packet = create_packet(type, data);
  printf("sending multicast type: %d\n", type);
  if ( data != NULL) {
    //                                    +4 (header) 
    return sendto(sd, (char *)&packet, strlen(data) + 4, 0, (struct sockaddr*)&msock, sizeof(msock));
  }
  else
    return sendto(sd, (char *)&packet, 4, 0, (struct sockaddr*)&msock, sizeof(msock));
}

int send_unicast(int type, char* data) {
  packet packet;
  packet = create_packet(type, data);
  int returnvalue;
  returnvalue = 0;

  for (int i = 0; i < MAX_MEMBERS; i++) {
    if ((browselist[i].socket != 0) //dont send to empty sockets
        && (strncmp(inet_ntoa(localip), browselist[i].ip, INET_ADDRSTRLEN) != 0 ) ) { //dont send to ourselves
      returnvalue = send(browselist[i].socket, (char *)&packet, MAX_MSG_LEN + 4, 0);
      #ifdef DEBUG
        printf("socket: %d\n", browselist[i].socket);
        printf("sending data: %s\n", data);
      #endif
    }
  }

  return returnvalue;
}

// Function to set a new state on the StateMachine
void setNewState(int state) {
  appl_state = state;
}

// Function to return current state
// TODO: obsolete?
int getState() {
  return appl_state;
}

// Function to set the global timer used for select
void setGlobalTimer(int sec, int usec) {
  //#ifdef DEBUG
  //  printf("sec: %d, usec: %d\n", sec, usec);
  //#endif
  globalTimer.tv_sec  = sec;
  globalTimer.tv_usec = usec;
}

packet create_packet(int type, char* data) {
  seqno++; //increment sequence number by one
 
  //#ifdef DEBUG
  //printf("DEBUG creating packet with seq no %d\n", seqno);
  //printf("DEBUG creating packet type: %d\n", type);
  //#endif

  packet packet;
  uint32_t header;
  uint16_t datalen;

  memset(packet.data, 0, MAX_MSG_LEN);

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

// Function to add a client into the BrowseList
void addToBrowseList(char* clientip, int i) {
  pdebug("adding item to browse list");
  int duplicate = 0;
  // Check if item already exists in browselist
  // starting with first slave
  for (int index = 1; index < i; index++) {
    if (!strcmp(browselist[index].ip,clientip)) {
      duplicate = 1;
    }
  }
  if (duplicate == 1) {
    pdebug("item already in browse list, not adding");
    // check if browselistlength was i before
    if (i == browselistlength -1) {
      // and decrement if necessary
      browselistlength--;
    }
  }
  else {
    if (strncmp(clientip, inet_ntoa(localip), INET_ADDRSTRLEN) == 0) {
      localindex = i; //local index in browselist
    }
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
}

void removeFromBrowseList(int i) {
#ifdef DEBUG
  printf("DEBUG removing entry %d from browselist\n", i);
#endif

  memset(browselist[i].name, 0, strlen(browselist[i].name));
  memset(browselist[i].ip, 0, strlen(browselist[i].ip));
   if ( browselist[i].socket != 0) {
#ifdef DEBUG
    printf("closing socket %d\n", i);
#endif
    close(browselist[i].socket);
    browselist[i].socket = 0;
  }
}

void reset_browselist() {
  for (int i = 0; i < browselistlength - 1; i++) {
    removeFromBrowseList(i);
  }
  browselistlength = 0;
}

int send_BrowseListItem(int index) {
  char data[64];
  char bllength[16];
  char blindex[16];
  char iplength[16];
  char ip[16];
  strncpy(ip, browselist[index].ip, 16);
  sprintf(blindex, "%d", index);
  sprintf(bllength, "%d", browselistlength);
  sprintf(iplength, "%d", (int)strlen(ip)); 
  strncpy(data, bllength, 16);
  strncat(data, ",", 1);
  strncat(data, blindex, 16);
  strncat(data, ",", 1);
  strncat(data, iplength, 16);
  strncat(data, ",", 1);
  strncat(data, ip, 16);

  #ifdef DEBUG
    printf("DEBUG sending browse list item: %s\n", data);
  #endif

  send_multicast(BROWSE_LIST, data);
  return 0;
 
}

int receive_BrowseListItem(char* data) {
  char bllength[16];
  char blindex[16];
  char iplength[16];
  char ip[16];

  strncpy(bllength, strtok(data, ","), 16);
  strncpy(blindex, strtok(NULL, ","), 16);
  strncpy(iplength, strtok(NULL, ","), 16);
  strncpy(ip, strtok(NULL, ","), 16);

  browselistlength == atoi(bllength);

  // if index is 0 then we are about to receive a new browselist
  if (atoi(blindex) == 0)
    reset_browselist();

  addToBrowseList(ip, atoi(blindex));

  #ifdef DEBUG
    printf("DEBUG received index %s of %s with iplength of %s, ip: %s\n", blindex, bllength, iplength, ip);
  #endif
  return 0;
}

void close_chat() {
  pdebug("quitting chat");
  reset_browselist(); //close all connections by resetting browselist

  //tell the group that we are leaving
  if (appl_state == I_AM_MASTER)
    send_multicast(LEAVE_GROUP_MASTER, NULL);
  else
    send_unicast(LEAVE_GROUP, NULL);

  close(sd); //close multicast socket
  close(s);  //close incoming unicast socket

  exit(0); //return successfull code
}
