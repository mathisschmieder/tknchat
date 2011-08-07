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

// Options for argument parsing
int option_index = 0;
const char* eth = NULL; 

// Define our file descriptor set
fd_set rfds;

// State machine
int appl_state = STATE_NULL;
struct timeval globalTimer;
int OS_Level = 0;
int maxreq;
int alive_req;
int masterdelay;

// Communication
int retval;
char buf[MAX_MSG_LEN];
int seqno;
int browselistlength = 0;
char host[1024];
in_addr localip;
int localindex;

// Character input
int ch;
char input[80];
int input_set;
  
// Windows
// output
int output_xstart, output_ystart, output_width, output_height;
int output_xpos, output_ypos;
// input
int input_xstart, input_ystart, input_width, input_height;
int input_xpos, input_ypos;
int state_xpos, state_ypos;
// debug
int debug_xstart, debug_ystart, debug_width, debug_height;

WINDOW *debug_win, *output_win, *input_win;

// The main method
int main(int argc, char** argv) {
  // Parse arguments from the command line
  parse_options(argc, argv);

  // Create ncurses user interface
  //WINDOW *output_win, *input_win;
  initscr();
  noecho();
  //cbreak();
  raw();
  // Hide cursor
  curs_set(0);
  // I need F1
  // TODO do i?
  keypad(stdscr, TRUE);   
  
  // never forget
  refresh();

  // Debug window
  debug_xstart = 0;
  debug_ystart = 0;
  #ifdef DEBUG
    debug_height = 10;
  #else
    debug_height = 0;
  #endif
  debug_width = COLS;
  debug_win = create_newwin(debug_height, debug_width, debug_ystart, debug_xstart, 1);

  scrollok(debug_win, true);
  //idlok(debug_win, true);
  wrefresh(debug_win);

  // Output window
  output_xstart = 0;
  output_ystart = debug_height;
  output_height = LINES - 3 - debug_height;
  output_width = COLS;
  // NOBORDER
  //output_win = create_newwin(output_height, output_width, output_ystart, output_xstart, 0);
  // BORDER
  output_win = create_newwin(output_height, output_width, output_ystart, output_xstart, 1);

  scrollok(output_win, true);
  //idlok(output_win, true);
  wrefresh(output_win);

  // Input window
  input_xstart = 0;
  input_ystart = LINES - 3;
  input_height = 3;
  input_width = COLS;
  input_win = create_newwin(input_height, input_width, input_ystart, input_xstart, 1);
  nodelay(input_win, true);
  wrefresh(input_win);

  // Define cursor

  int output_xpos = output_xstart+1;
  int output_ypos = output_ystart+1;

  // Input
  memset(input, 0, 80);
  input_set = 0;

  // Multicast packet
  local_packet mc_packet;

  // TODO maybe take a better seed
  srand( time(NULL) ); 
  OS_Level = rand() % 65535 + 1;

  host[1023] = '\0';
  gethostname(host, 1023);

  if (eth == NULL) 
    eth = "eth0"; 

  localip = getIP(eth);

  sd = setup_multicast();
  s = setup_unicast_listen();
  init_fdSet(&rfds);

  //globalTimer.tv_sec = 1;
  //globalTimer.tv_usec = 0;
  setGlobalTimer(1,0);

  maxreq = 5;

  #ifdef DEBUG
    pdebug(" OS_Level %d\n", OS_Level);
  #endif


  // Open thread for input

  pthread_t t1;
  poutput("thread starting\n");
  if ( pthread_create(&t1, NULL, get_input, (void *)" execute task 1\n") != 0 ) {
    poutput("pthread_create() error");
    abort();
  }

  //int count = 0;
  //while (true) {
  //  count++;
  //  pdebug(" debug %d\n", count);
  //  poutput(" output %d\n", count);
  //  sleep(1);
  //  //usleep(500000);
  //}

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

        pdebug(" incoming unicast connection from %s\n", inet_ntoa(client.sin_addr));
      
        //find out where the connection came from
        for (int i = 0; i < MAX_MEMBERS; i++) {
          if (strncmp(inet_ntoa(client.sin_addr), browselist[i].ip, INET_ADDRSTRLEN) == 0 ) {
            browselist[i].socket = newsock;
            valid = 1;
            break;
          }
        }
        if (valid == 0) {
          pdebug("closing non-authorized unicast connection\n");
          close(newsock);
        }
      } 
      // TODO obsolete
      //else if (FD_ISSET(0, &rfds)) {
      //  // keyboard input
      //  char buffer[MAX_MSG_LEN];
      //  char * p;

      //  memset(buffer, 0, MAX_MSG_LEN); //clear buffer
      //  fgets(buffer, MAX_MSG_LEN, stdin);     

      //  
      //  //remove trailing newline character                                       
      //  if ((p = strchr(buffer, '\n')) != NULL)
      //    *p = '\0';

      //  if (strncmp("/", &buffer[0], 1) == 0) { // input with / is a command
      //    if (strncmp("quit", &buffer[1], 5) == 0)
      //      close_chat();
      //  }

      //  send_unicast(DATA_PKT,buffer);
      //}
      for (int i = 0; i < MAX_MEMBERS; i++) {
        if (browselist[i].socket > 0) {
          if (FD_ISSET(browselist[i].socket, &rfds)) {
            local_packet uc_packet;
            packet uc_recv;
            memset(uc_recv.data, 0, strlen(uc_recv.data));
            recv(browselist[i].socket, &uc_recv, sizeof(uc_recv), 0);

            uc_packet = receive_packet(uc_recv);
            //printf("unicast received type: %d\n", uc_packet.type);
            //printf("unicast received from: %s\n", browselist[i].name);

            if ((uc_packet.type == DATA_PKT) && ((int)strlen(uc_packet.data) > 0)) {
              poutput(" <%s> %s\n", browselist[i].name, uc_packet.data);
            } else if (uc_packet.type == LEAVE_GROUP) { //only null-data packet on unicast is LEAVE_GROUP 
              poutput(" >>> %s left the building\n", browselist[atoi(mc_packet.data)].name);
              browselistlength = removeFromBrowseList(i);
              if (appl_state == I_AM_MASTER) { //inform other clients of the part
                char partindex[3];
                sprintf(partindex, "%d", i);
                send_multicast(LEAVE_GROUP, partindex); 
              }
            } else {
              //TODO explanation why this is necessary
              browselistlength = removeFromBrowseList(i);
            }
          }
        }
      }
    } 

    if (input_set == 1) {
      poutput(" <%s> %s\n", host, input);
      send_unicast(DATA_PKT,input);
      memset(input, 0, 80);
      input_set = 0;
    }
    // STATE MACHINE
    switch(appl_state) {

      case STATE_NULL:
        // a: send_SEARCHING_MASTER
        send_multicast(SEARCHING_MASTER, inet_ntoa(localip));
        setNewState(STATE_INIT);
        break;
      
      case STATE_INIT:
        if ( maxreq > 0) {
          maxreq--;
          // e: rcvd_I_am_Master
          // a: send_get_browse_list
          if (mc_packet.type == I_AM_MASTER) {
            pdebug(" master found\n");
            maxreq = 6;
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
        // e: rcvd_browse_list 
        // a: manage_member_list 
        // a: establishConn
        if (mc_packet.type == BROWSE_LIST) {
          maxreq = 5;
          browselistlength = receive_BrowseListItem(mc_packet.data);
          setNewState(STATE_BROWSELIST_RCVD);
        } 
        else if (mc_packet.type == GET_MEMBER_INFO) {
          maxreq = 6;
          pdebug(" SENDING MEMBER INFO\n");
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
        // TODO nice way of handling master dropouts
        // Trying to periodically refresh browselist 
        // and see if master is still there

        if ( mc_packet.type == (int)NULL ) { //if there is no data, increment alive_req to check if the master still lives
          // FIXME
         // if (alive_req < 25) {
         //   alive_req++;
         // } else {
         //   pdebug(" master dead!\n");
         //   setNewState(STATE_FORCE_ELECTION);
         // }
        } else if ((mc_packet.type == CTRL_PKT) && (mc_packet.datalen == 0)) { //master ping, respond to it with a control packet and our ip
          alive_req = 0;
          pdebug(" master alive\n");
          send_multicast(CTRL_PKT, inet_ntoa(localip));
        } else if (mc_packet.type == GET_MEMBER_INFO) { //master requests our info. supply them and go into STATE_MASTER_FOUND to wait for new browselist
          maxreq = 6; //possible FIXME for segfaults
          pdebug(" SENDING MEMBER INFO\n");
          send_multicast(SET_MEMBER_INFO, inet_ntoa(localip));
          //TODO better handling of waiting time until the new master is ready <- this still necessary? 
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
          receive_BrowseListItem(mc_packet.data);
        } else if (mc_packet.type == LEAVE_GROUP) {
          //remove client from browselist
          poutput(" >>> %s left the building\n", browselist[atoi(mc_packet.data)].name);
          removeFromBrowseList(atoi(mc_packet.data));
        }
        if ( setup_unicast() < 0) {
          pdebug(" error setting up unicast connections, requesting new browse list\n");
          reset_browselist();
          maxreq = 6;
          setNewState(STATE_MASTER_FOUND);
        }
        break;

      case STATE_FORCE_ELECTION:
        // e: rcvd_master_level > mine
        // a: Am_I_the_Master? No
        if ((mc_packet.type == I_AM_MASTER) || 
            ( (mc_packet.type == MASTER_LEVEL) && (ntohl(atoi(mc_packet.data)) > OS_Level) )) {
          //TODO better handling of waiting time until the new master is ready
          maxreq = 5;
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
          pdebug(" assuming I am master\n");
          maxreq = 5;
          setNewState(STATE_I_AM_MASTER);
          // Wait 3 cycles until we are sure that we are the master
          masterdelay = 8; 
        }
        break;

      case STATE_I_AM_MASTER:
        // Have we received a multicast packet?
        if ( mc_packet.type == (int)NULL ) { 
          if (masterdelay > 1)
            pdebug(" masterdelay: %d\n", masterdelay);
            masterdelay--;
          if (masterdelay == 5) {
            // Now we are the master
            masterdelay--; 
            // Initialize BrowseList
            reset_browselist();
            pdebug(" adding myself\n");
            browselistlength = addToBrowseList(inet_ntoa(localip));
            // Ask all clients for their credentials
            //send_multicast(BROWSE_LIST, 
            send_multicast(GET_MEMBER_INFO,NULL); 
          } else if (masterdelay == 1) {
            //the clients had enough time to send us their infos, now we send out the browselist
            for (int i = 0; i < browselistlength; i++) {
              pdebug(" Sending browselistentry %d", i);
              send_BrowseListItem(i);
              masterdelay = 0;
            }
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
            masterdelay = 0; //prevent sending the browselist a second time later on
            for (int i = 0; i < browselistlength; i++) {
              pdebug(" sending browselistentry %d\n", i);
              send_BrowseListItem(i);
            }
          } 
          // e: rcvd_set_member_info
          // a: manage_member_list
          else if (mc_packet.type == SET_MEMBER_INFO) {
            // manage_member_list
            browselistlength = addToBrowseList(mc_packet.data);
          } 
          // e: rcvd_searching_master
          // a: send_I_am_Master
          else if ( mc_packet.type == SEARCHING_MASTER ) {
            send_multicast(I_AM_MASTER, NULL);
            browselistlength = addToBrowseList(mc_packet.data);
          } else if ( mc_packet.type == FORCE_ELECTION ) {
            setNewState(STATE_FORCE_ELECTION);
          } 
        }

        // FIXME

       // if (maxreq > 0) {
       //   maxreq--;
       //   if (alive_req > 1) {
       //     if ((mc_packet.type == CTRL_PKT) && (mc_packet.datalen > 0)) {
       //       alive_req--;
       //     }
       //   }
       //   // All slaves replied
       //   else if (alive_req == 1) {
       //     alive_req = 0;
       //     pdebug(" all slaves alive\n");
       //   }
       // }
       // else {
       //   if (alive_req > 0) {
       //     pdebug(" slave dead!\n");
       //     // TODO best way to handle this?
       //     alive_req = 0;
       //     maxreq = 10;
       //     masterdelay = 6;
       //   }
       //   else {
       //     maxreq = 10;
       //     alive_req = browselistlength;
       //     pdebug(" master ping!\n");
       //     send_multicast(CTRL_PKT, NULL);
       //   }
       // }
        break;
    }
    init_fdSet(&rfds);
    setGlobalTimer(1,0);
    wrefresh(output_win);
  }
  close_chat();
  return 0;
}

// Function to output debug messages
// when the DEBUG option is set
void pdebug(const char* fmt, ...) {
  #ifdef DEBUG
    va_list ap;
    va_start(ap, fmt);
    vw_printw(debug_win, fmt, ap); 
    va_end(ap);
    box(debug_win, 0, 0);
    wrefresh(debug_win);
  #endif

}

void poutput(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vw_printw(output_win, fmt, ap); 
    va_end(ap);
    box(output_win, 0, 0);
    wrefresh(output_win);
}

void *get_input(void *arg) {
  int index = 0;
  char local_input[80];
  memset(input, 0, 80);
  input_ypos = 1;
  input_xpos = 1;
  mvwprintw(input_win, input_ypos, input_xpos, "%s", input);
  
  // Continue until we catch CTRL-C
  while((ch = getch()) != 3) {
    // ENTER, BACKSPACE, frequently used characters
    if ((ch == 10) || (ch == 263) || (( ch >= 32) && (ch <= 126))) {
      switch(ch) {
        case KEY_BACKSPACE:
          if( index > 0) {
            index--;
            local_input[index] = (char)NULL;
            input_xpos--;
            mvwaddch(input_win, input_ypos, input_xpos, ' ');
            wrefresh(input_win);
          }
        break;
        case '\n':
          if (( index > 0) && (input_set == 0)) {
            //poutput(" > %s\n", local_input);
            index = 0;
            while (input_xpos > 1) {
              input_xpos--;
              mvwaddch(input_win, input_ypos, input_xpos, ' ');
            }
            
            wrefresh(input_win);
            strncpy(input, local_input, 80);
            input_set = 1;
            memset(local_input, 0, 80);
          }
        break;
        default:
          local_input[index++] = ch;
          mvwaddch(input_win, input_ypos, input_xpos, ch);
          wrefresh(input_win);
          input_xpos++;
        break;
      }
    }
    else {
      pdebug(" keycode: %d\n", ch);
    }
  }
  //poutput((char *)arg);
  close_chat();
  return NULL;
}


// Function to output program version
void version() 
{
  // TODO git version?
  printf("tknchat v0.5-schlagmichtot\n");
}

// Function for basic help
void usage()
{
  printf("usage:\n");
  printf(" -h --help\t print this help\n");
  printf(" -v --version\t show version\n");
  printf(" -i --interface\t set primary interface (default eth0)\n");
}

// Function for parsing and handling command line arguments 
void parse_options(int argc, char** argv) {
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
  // handled by ncurses now
  //FD_SET(0, fds);
  // add multicast 
  FD_SET(sd, fds);
  // add unicast incoming port
  FD_SET(s, fds);
  // add unicast connections
  for (int i = 0; i < MAX_MEMBERS; i++) 
    if (browselist[i].socket > 0) 
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

  pdebug(" Multicast set up\n");
  return sd;
}

int setup_unicast_listen() {
  int s;
  struct sockaddr_in srv;

  pdebug(" Setting up incoming socket\n");

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
    if ((browselist[i].socket == -1) 
        && (strncmp(inet_ntoa(localip), browselist[i].ip, INET_ADDRSTRLEN) != 0 )) { // this should be obsolete
        pdebug(" opening connection to %s, i is %d\n", browselist[i].ip, i);
      newsock = socket(AF_INET, SOCK_STREAM, 0);
      if (newsock < 0) {
        perror("Error opening unicast socket, closing chat");
        close_chat();
      }

      options.sin_addr.s_addr = inet_addr(browselist[i].ip);
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
  //printf("sending multicast type: %d\n", type);
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
    if ((browselist[i].socket > 0) //dont send to empty sockets
        && (strncmp(inet_ntoa(localip), browselist[i].ip, INET_ADDRSTRLEN) != 0 ) ) { //dont send to ourselves
      returnvalue = send(browselist[i].socket, (char *)&packet, MAX_MSG_LEN + 4, 0);
        pdebug("socket: %d\n", browselist[i].socket);
        pdebug("sending data: %s\n", data);
    }
  }

  return returnvalue;
}

// Function to set a new state on the StateMachine
void setNewState(int state) {
  appl_state = state;

  char cstate[18];
  switch(appl_state) {
    case STATE_NULL:
      strncpy(cstate, "[NULL]", 18);
      break;
    case STATE_INIT:
      strncpy(cstate, "[INIT]", 18);
      break;
    case STATE_FORCE_ELECTION:
      strncpy(cstate, "[FORCE_ELECTION]", 18);
      break;
    case STATE_NO_MASTER:
      strncpy(cstate, "[NO_MASTER]", 18);
      break;
    case STATE_I_AM_MASTER:
      strncpy(cstate, "[I_AM_MASTER]", 18);
      break;
    case STATE_ELECTION:
      strncpy(cstate, "[ELECTION]", 18);
      break;
    case STATE_MASTER_FOUND:
      strncpy(cstate, "[MASTER_FOUND]", 18);
      break;
    case STATE_IDLE:
      strncpy(cstate, "[IDLE]", 18);
      break;
    case STATE_BROWSELIST_RCVD:
      strncpy(cstate, "[BROWSELIST_RCVD]", 18);
      break;
  }
  state_ypos = 0;
  state_xpos = COLS -strlen(cstate) -2;
  box(input_win, 0 , 0);    
  mvwprintw(input_win, state_ypos, state_xpos, "%s", cstate);
  wrefresh(input_win);
}

// Function to set the global timer used for select
void setGlobalTimer(int sec, int usec) {
  globalTimer.tv_sec  = sec;
  globalTimer.tv_usec = usec;
}

packet create_packet(int type, char* data) {
  // TODO: seqno % 255
  seqno++; //increment sequence number by one

  packet packet;
  uint32_t header;
  uint16_t datalen;

  memset(packet.data, 0, MAX_MSG_LEN);
  // Only set datalen if there is data
  if (data != NULL) { 
    datalen = strlen(data);
    strncpy(packet.data, data, datalen);
  } else
    datalen = 0;

  // Headerformat
  // 32Bit = VERSION 2    TYPE 5      OPTIONS 1  SEQNO 8      DATALEN 16 
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
  //pdebug(" Received packet, type: %d\n", local_packet.type);

  if (local_packet.datalen != 0) {
    strncpy(local_packet.data, packet.data, sizeof(packet.data));
  }

  return local_packet;
}

// Function to add a client to the BrowseList
void addToBrowseList(char* clientip, int i) {
  pdebug(" adding item to browse list\n");
  int duplicate = 0;
  // Check if item already exists in browselist
  // starting with first slave
  for (int index = 1; index < i; index++) {
    if (!strcmp(browselist[index].ip,clientip)) {
      duplicate = 1;
    }
  }
  if (duplicate == 1) {
    pdebug(" item already in browse list, not adding\n");
    // Check if browselistlength was i before...
    if (i == browselistlength -1) {
      // ...and decrement if necessary
      browselistlength--;
    }
  }
  else {
    if (strncmp(clientip, inet_ntoa(localip), INET_ADDRSTRLEN) == 0) {
      // Local index in browselist
      localindex = i; 
    }
    strncpy(browselist[i].ip, clientip, INET_ADDRSTRLEN);
    hostent* host;
    in_addr ip;
    ip.s_addr = inet_addr(clientip);
    host = gethostbyaddr((char*)&ip, sizeof(ip), AF_INET);
    strncpy(browselist[i].name, host->h_name, strlen(host->h_name));
    //pdebug(" IP: %s\n", browselist[i].ip);
    //pdebug(" Host: %s\n", browselist[i].name);
  }
}

// Function to append a client to the BrowseList
int addToBrowseList(char* clientip) {
  pdebug(" appending item to browse list\n");

  for (int index = 0; index < browselistlength - 1; index++) {
    if (!strcmp(browselist[index].ip,clientip)) {
      return -1;
    }
  }

  int i = browselistlength;
  if (strncmp(clientip, inet_ntoa(localip), INET_ADDRSTRLEN) == 0) {
    // Local index in browselist
    localindex = i; 
  }
  strncpy(browselist[i].ip, clientip, INET_ADDRSTRLEN);
  hostent* host;
  in_addr ip;
  ip.s_addr = inet_addr(clientip);
  host = gethostbyaddr((char*)&ip, sizeof(ip), AF_INET);
  strncpy(browselist[i].name, host->h_name, strlen(host->h_name));

  i++;
  return i;
}

// Function to remove a single client from the BrowseList 
int removeFromBrowseList(int i) {
  close(browselist[i].socket); // Clean up the socket
  
  if ( i == browselistlength - 1) {
    browselist[i].socket = -1;
    memset(browselist[i].name, 0, 1024);
    memset(browselist[i].ip, 0, INET_ADDRSTRLEN);
  } else {
    // Move all remaining entries one down
    for (int index = i; index < browselistlength - 1; index++) {
      strncpy(browselist[i].name, browselist[i+1].name, 1024);
      strncpy(browselist[i].ip, browselist[i+1].ip, INET_ADDRSTRLEN);
      browselist[i].socket = browselist[i+1].socket;
    }
  }

  return browselistlength - 1;
}

// Function to reset the BrowseList
void reset_browselist() {
  for (int i = 0; i < MAX_MEMBERS - 1; i++) {
    if (browselist[i].socket > 0)
      close(browselist[i].socket);

    browselist[i].socket = -1;
    memset(browselist[i].name, 0, 1024);
    memset(browselist[i].ip, 0, INET_ADDRSTRLEN);
  }
  browselistlength = 0;
}

// Function to send a single BrowseListItem
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

  //pdebug(" Sending BrowseList item: %s\n", data);

  send_multicast(BROWSE_LIST, data);
  return 0;
 
}

// Function to receive and handle BrowseListItems
int receive_BrowseListItem(char* data) {
  char bllength[16];
  char blindex[16];
  char iplength[16];
  char ip[16];

  strncpy(bllength, strtok(data, ","), 16);
  strncpy(blindex, strtok(NULL, ","), 16);
  strncpy(iplength, strtok(NULL, ","), 16);
  strncpy(ip, strtok(NULL, ","), 16);

  // If index is 0 we are about to receive a new BrowseList
  if (atoi(blindex) == 0)
    reset_browselist();

  addToBrowseList(ip, atoi(blindex));

  return atoi(bllength);
}

// Exit function
void close_chat() {
  poutput(" >> quitting chat\n");
  // Close all connections by resetting browselist
  reset_browselist(); 

  // Tell the group that we are leaving
  if (appl_state == I_AM_MASTER)
    send_multicast(LEAVE_GROUP_MASTER, NULL);
  else
    send_unicast(LEAVE_GROUP, NULL);

  // Close multicast socket
  close(sd);
  // Close incoming unicast socket
  close(s);

  // Clean up interface
  destroy_win(debug_win);
  destroy_win(output_win);
  destroy_win(input_win);
  endwin();
  // Exit gracefully
  exit(0);  
}

// Function to create a new ncurses window
WINDOW *create_newwin(int height, int width, int ystart, int xstart, int border) { 
  WINDOW *local_win;
  local_win = newwin(height, width, ystart, xstart);
  if (border == 1) {
    box(local_win, 0 , 0);    
  }
  wrefresh(local_win);

  return local_win;
}

// Function to destroy a ncurses window
void destroy_win(WINDOW *local_win) { 
  wborder(local_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
  wrefresh(local_win);
  delwin(local_win);
}

