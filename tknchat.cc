/*
** Technische Universitaet Berlin
** Lehrstuhl fuer Kommunikationsnetze
**
** Praktikum Kommunikationsnetze
** Block D - Chattool
**
** Mathis Schmieder - 316110
** Konstantin Koslowski - 316955
**
** Source file
*/

// Include the header file
#include "tknchat.h"

/* The main method
 *
 * Arguments:
 * int argc: number of arguments passed at program start
 * char** argv: character array of arguments
 * 
 * Return value:
 * int: >=0 on successfull termination, -1 on malicious termination
 *      This is unused and mainly here to make gcc happy
 */
int main(int argc, char** argv) {
  // Parse arguments from the command line
  parse_options(argc, argv);

  // Create ncurses user interface
  initscr();
  noecho();
  raw();
  // Hide cursor
  curs_set(0);
  // I need F1
  // TODO do i? @konni
  keypad(stdscr, TRUE);   
  
  // never forget
  refresh();

  // Debug window - this will only be displayed if compiled with -DDEBUG
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
  wrefresh(debug_win);

  // Output window
  output_xstart = 0;
  output_ystart = debug_height;
  output_height = LINES - 3 - debug_height;
  output_width = COLS;
  output_win = create_newwin(output_height, output_width, output_ystart, output_xstart, 1);

  scrollok(output_win, true);
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

  // OS_Level generation 
  // Initialize the random seed using current time
  srand( time(NULL) ); 
  // Set OS_Level to a random value between 1 and 65535
  OS_Level = rand() % 65535 + 1; 

  // Set the interface for all unicast communication. Defaults to eth0 and may be specified via programm argument -i
  if (eth == NULL) 
    eth = "eth0"; 

  // Get our local IP
  localip = getIP(eth);

  // Set up multicast and incoming unicast sockets
  sd = setup_multicast();
  s = setup_unicast_listen();
  // And add them to the file descriptor set
  init_fdSet(&rfds);

  // Initialize the global timer to 1 second
  setGlobalTimer(1,0);

  // Set the retry counter to 5
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
  
  for (;;) { // main loop
    // READ FROM SOCKETS 
    // select monitors the file descriptors in fdset and returns a positive integer if
    // there is data available
    retval = select(sizeof(&rfds)*8, &rfds, NULL, NULL, (struct timeval*)&globalTimer);
    
    // select() should always return a value greater or equal to 0
    // If the return value is <0 a socket got bad and the program should be quit
    // Ideally this should never happen!
    if (retval  < 0) {
      pdebug(" something really horrible happened, closing the chat.\n");
      poutput(" This is embarrassing. Force-closing the chat\n");
      close_chat();
    }

    // Clearing and initializing multicast packet
    mc_packet.type = (int)NULL;
    memset(mc_packet.data, 0, strlen(mc_packet.data));


    // Get data from sockets 
    if (retval > 0) {
      // FD_ISSET returns 1 if there is data to be received in the specified socket
      if (FD_ISSET(sd, &rfds)) { // Is there data on the multicast socket?
        packet mc_recv;
        memset(mc_recv.data, 0, strlen(mc_recv.data));  // Initialize and clear mc_recv
        recv(sd, &mc_recv, sizeof(mc_recv), 0);         // and receive data

        mc_packet = receive_packet(mc_recv); // Decode raw data into local_packet struct
      } 
      else if (FD_ISSET(s, &rfds)) {
        // We have a new visitor aka data on unicast listening socket
        int newsock, cli_len, valid;
        struct sockaddr_in client;
        cli_len = sizeof(client);
        valid = 0;
        // accept() accepts a connection to a local, passive open socket and returns an active open one
        newsock = accept(s, (sockaddr *)&client, (socklen_t *)&cli_len);

        pdebug(" incoming unicast connection from %s\n", inet_ntoa(client.sin_addr));
      
        // Scan the browse list and compare the new connection's source to all known clients
        // We will only accept connections from known members
        for (int i = 0; i < browselistlength; i++) { 
          if (strncmp(inet_ntoa(client.sin_addr), browselist[i].ip, INET_ADDRSTRLEN) == 0 ) { 
            // We found the source's IP in the browse list
            browselist[i].socket = newsock; // Accept and save active socket
            valid = 1;
            break; // End for-loop
          }
        }
        if (valid == 0) { 
          // Source's IP couldn't be found in the browse list, disconnecting
          pdebug("closing non-authorized unicast connection\n");
          close(newsock);
        }
      } 

      // Check all member's sockets for data
      for (int i = 0; i < browselistlength; i++) { 
        if (browselist[i].socket > 0) { 
          // Only check on connected sockets
          if (FD_ISSET(browselist[i].socket, &rfds)) {
            local_packet uc_packet; // Declare  local_packet packet
            packet uc_recv;         // Declare raw packet
            // Memory gets reused so make sure there is no garbage left
            memset(uc_recv.data, 0, strlen(uc_recv.data)); 
            // Read from unicast stream socket
            read(browselist[i].socket, &uc_recv, sizeof(uc_recv)); 

            uc_packet = receive_packet(uc_recv);

            if ((uc_packet.type == DATA_PKT) && ((int)strlen(uc_packet.data) > 0)) { 
              // Receive chat message and display it
              poutput(" <%s> %s\n", browselist[i].name, uc_packet.data);
            } else if (uc_packet.type == LEAVE_GROUP) {
              poutput(" >>> %s has left the building\n", browselist[i].name);
              browselistlength = removeFromBrowseList(i);
              // The master SHOULD be the only one receiving this. Still checking just to be sure
              if (appl_state == I_AM_MASTER) { 
                char partindex[3];
                sprintf(partindex, "%d", i);
                // Inform all slaves of client leave
                send_multicast(LEAVE_GROUP, partindex);  
              }
            } else { 
              // NULL-data packet received with a type != LEAVE_GROUP. This should not happen
              pdebug(" strange data from %s\n", browselist[i].name);
            }
          }
        }
      }
    } 

    // Check if data has been set in the ncurses thread
    if (input_set == 1) { 
      // Local echo of chat message
      poutput(" <%s> %s\n", browselist[localindex].name, input); 
      // Send chat message to other members
      send_unicast(DATA_PKT,input); 
      // Clear input char array
      memset(input, 0, 80); 
      input_set = 0;
    }

    // STATE MACHINE
    // This pretty much complies with the given state machine diagram
    switch(appl_state) {
      case STATE_NULL:
        // a: send_SEARCHING_MASTER
        // Supply the master with our IP
        send_multicast(SEARCHING_MASTER, inet_ntoa(localip)); 
        setNewState(STATE_INIT);
        break;
      
      case STATE_INIT:
        if ( maxreq > 0) {  // Check if the retry counter is > 0
          maxreq--;         // and decrease it
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
            // Retry counter is >0 but no data received, searching for master again
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
          maxreq = 5; // Reset retry counter
          browselistlength = receive_BrowseListItem(mc_packet.data);
          setNewState(STATE_BROWSELIST_RCVD);
        } 
        else if (mc_packet.type == GET_MEMBER_INFO) {
          // Received request to send our infos
          maxreq = 6;
          pdebug(" SENDING MEMBER INFO\n");
          send_multicast(SET_MEMBER_INFO, inet_ntoa(localip));
        }
        else if (mc_packet.type == (int)NULL ) { 
          // Only do this if there is no received packet
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
        // e: rcvd_get_member_info
        // a: send_member_info
        // s: STATE_MASTER_FOUND
        if (mc_packet.type == GET_MEMBER_INFO) { 
          // Master requests our info. supply them and go into STATE_MASTER_FOUND to wait for new browselist
          maxreq = 6; // Reset retry counter 
          pdebug(" SENDING MEMBER INFO\n");
          send_multicast(SET_MEMBER_INFO, inet_ntoa(localip));
          setNewState(STATE_MASTER_FOUND);
        } 
        // e: rcvd_leave_group_master
        // a: remove master from browse list
        // a: send_force_election
        // s: STATE_FORCE_ELECTION
        else if (mc_packet.type == LEAVE_GROUP_MASTER) {
          maxreq = 5;
          removeFromBrowseList(0);
          send_multicast(STATE_FORCE_ELECTION, NULL);
          setNewState(STATE_FORCE_ELECTION);
        }
        // e: rcvd_browse_list
        // e: rcvd_leaved
        // a: manage_member_list
        else if ((mc_packet.type == BROWSE_LIST)) {
          browselistlength = receive_BrowseListItem(mc_packet.data);
        } 
        // e: rcvd_leave_group
        // a: remove member from browse list
        else if (mc_packet.type == LEAVE_GROUP) {
          poutput(" >>> %s has left the building\n", browselist[atoi(mc_packet.data)].name);
          browselistlength = removeFromBrowseList(atoi(mc_packet.data));
        }
        // Set up unicast connections. If the return value is < 0 then something went wrong
        // If that is the case, clear browse list and request a new one 
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
          maxreq = 5;
          setNewState(STATE_MASTER_FOUND);
          break;
        }
        // e: Timeout or rcvd_master_level < mine 
        // a: Assume we are master but wait a little more before we publish it 
        else {
          // Create char to hold the OS_LEVEL (8 digits plus \0)
          char char_OS_Level[sizeof(OS_Level)*8+1];
          sprintf(char_OS_Level, "%d", htonl(OS_Level));
          // And send it via multicast
          send_multicast(MASTER_LEVEL, char_OS_Level);
          pdebug(" assuming I am master\n");
          maxreq = 5;
          setNewState(STATE_I_AM_MASTER);
          // Wait a couple of cycles until we are sure that we are the master
          masterdelay = 8; 
        }
        break;

      case STATE_I_AM_MASTER:
        // Have we received a multicast packet?
        if ( mc_packet.type == (int)NULL ) { 
          if (masterdelay > 1)
            masterdelay--;
          if (masterdelay == 5) {
            // 8-5=3 cycles without data
            // We can safely assume that we are the master 
            masterdelay--; 
            // Initialize BrowseList
            reset_browselist();
            // Add ourself to the browse list
            browselistlength = addToBrowseList(inet_ntoa(localip));
            // Ask all clients for their credentials
            send_multicast(GET_MEMBER_INFO,NULL); 
          } else if (masterdelay == 1) {
            // 4-1=3 cycles without data
            // We can safely assume that all members have sent their info
            // Send out the new browse list
            for (int i = 0; i < browselistlength; i++) {
              pdebug(" Sending browselistentry %d\n", i);
              send_BrowseListItem(i);
              // Prevent the master from re-requesting and resending all data
              masterdelay = 0;
            }
          }
        } else {
            // e: rcvd_master_level greater than mine
            // a: am_I_the_Master? No
          if ((mc_packet.type == MASTER_LEVEL) && (ntohl(atoi(mc_packet.data)) > OS_Level)) {
            maxreq = 5;
            reset_browselist();
            setNewState(STATE_MASTER_FOUND);
            break;
          }
          // e: rcvd_get_browselist
          // a: send_browselist
          else if (mc_packet.type == GET_BROWSE_LIST) {
            // Prevent sending the browselist a second time later on without being asked to
            masterdelay = 0; 
            for (int i = 0; i < browselistlength; i++) {
              pdebug(" sending browselistentry %d\n", i);
              send_BrowseListItem(i);
            }
          } 
          // e: rcvd_set_member_info
          // a: manage_member_list
          else if (mc_packet.type == SET_MEMBER_INFO) {
            // Declare a char array for the received IP
            // There may be garbage behind the IP in the received data
            // Only copy mc_packet.datalen chars to eliminate garbage
            char receivedip[INET_ADDRSTRLEN];
            memset(receivedip, 0, INET_ADDRSTRLEN);
            strncpy(receivedip, mc_packet.data, mc_packet.datalen);
            // Append to browse list
            // Return value is -1 if entry already present
            int newbllength = addToBrowseList(receivedip);
            if (newbllength > 0)
              browselistlength = newbllength;
          } 
          // e: rcvd_searching_master
          // a: send_I_am_Master
          else if ( mc_packet.type == SEARCHING_MASTER ) {
            char receivedip[INET_ADDRSTRLEN];
            memset(receivedip, 0, INET_ADDRSTRLEN);
            strncpy(receivedip, mc_packet.data, mc_packet.datalen);
            browselistlength = addToBrowseList(receivedip);
            send_multicast(I_AM_MASTER, NULL);
          } 
          // e: rcvd_force_election
          // Clear browse list and go into election state
          else if ( mc_packet.type == FORCE_ELECTION ) {
            reset_browselist();
            setNewState(STATE_FORCE_ELECTION);
          } 
        }
        break;
    }
    // Re-Initialize file descriptor set, refresh timer and output window
    init_fdSet(&rfds);
    setGlobalTimer(1,0);
    wrefresh(output_win);
  }
  // The loop should never close, but if it does, exit the program cleanly
  close_chat();
  return 0;
}

/* Function to output debug messages to ncurses thread
 * when the DEBUG option is set
 *
 * Arguments
 * const char* fmt: Message to display, same syntax as printf()
 */
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

/* Function to output main data to ncurses thread
 *
 * Arguments
 * const char* fmt: Message to display, same syntax as printf()
 */
void poutput(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vw_printw(output_win, fmt, ap); 
    va_end(ap);
    box(output_win, 0, 0);
    wrefresh(output_win);
}

/* Function to get input from ncurses thread
 *
 * TODO @konni
 *
 */
void *get_input(void *arg) {
  int index = 0;
  char local_input[80];
  memset(local_input, 0, 80);
  input_ypos = 1;
  input_xpos = 1;
  mvwprintw(input_win, input_ypos, input_xpos, "%s", input);
  
  // Continue until we catch CTRL-C
  while((ch = getch()) != 3) {
    // ENTER
    if (ch == 10) {
      // Input available and ready to send
      if (( index > 0) && (input_set == 0)) {
        // Control Sequence
        if (local_input[0] == 47) {
          if (strncmp("quit", &local_input[1], 5) == 0) {
            pdebug(" control sequence: quit\n");
            ungetch(3);
          }
          else {
            if (strncmp("who", &local_input[1], 4) == 0) {
              pdebug(" control sequence: who\n");
              display_browselist();
            }
            else {
              if (strncmp("help", &local_input[1], 5) == 0) {
                pdebug(" control sequence: help\n");
                display_help();
              }
              else
                poutput(" >>> Command not recognized.\n\tEnter /help for a list of commands\n");
              }
          }
        }
        // Normal input
        else {
          strncpy(input, local_input, 80);
          input_set = 1;
        }
        // Clear input and buffer
        index = 0;
        while (input_xpos > 1) {
          input_xpos--;
          mvwaddch(input_win, input_ypos, input_xpos, ' ');
        }
        
        wrefresh(input_win);
        memset(local_input, 0, 80);
      }
    }
    // BACKSPACE
    else if ( ch == 263 ) {
      if( index > 0) {
        index--;
        local_input[index] = (char)NULL;
        input_xpos--;
        mvwaddch(input_win, input_ypos, input_xpos, ' ');
        wrefresh(input_win);
      }
    }
    // frequently used characters and umlauts
    else if (( ch >= 32) && (ch <= 126)) {
      local_input[index++] = ch;
      mvwaddch(input_win, input_ypos, input_xpos, ch);
      wrefresh(input_win);
      input_xpos++;
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
  printf("tknchat v0.9\n");
}

// Function to output basic help
void usage()
{
  printf("usage:\n");
  printf(" -h --help\t print this help\n");
  printf(" -v --version\t show version\n");
  printf(" -i --interface\t set primary interface (default eth0)\n");
}

/* Function for parsing program arguments
 *
 * Arguments
 * int argc: Number of arguments
 * char** argv: Array with arguments
 */
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

/* Function to get an interface's IP address
 *
 * Arguments
 * const char* eth: Linux network interface name
 *
 * Return Value
 * in_addr sin_addr: Interface's IP address in struct in_addr format
 */
in_addr getIP(const char* eth) {
  struct ifaddrs * ifAddrStruct=NULL;
  struct ifaddrs * ifa=NULL;
  struct sockaddr_in * sockaddr;

  // Get all available network interfaces as a linked list
  getifaddrs(&ifAddrStruct);
 
  // Traverse linked list and search for correct interface
  for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
    if ((ifa->ifa_addr != NULL) 
        // We only use IPv4
        && (ifa->ifa_addr->sa_family==AF_INET) 
        && (strcmp(ifa->ifa_name, eth) == 0)) {
        sockaddr = (struct sockaddr_in *)(ifa->ifa_addr);
       break;
    }
  }
  // Free memory allocated by getifaddrs()
  freeifaddrs(ifAddrStruct);

  return sockaddr->sin_addr;
}

/* Function to initialize file descriptor set
 *
 * Arguments
 * fd_set* fds: file descriptor set to initialize
 */
void init_fdSet(fd_set* fds) {
  // reset fds
  FD_ZERO(fds);
  
  // add multicast 
  FD_SET(sd, fds);
  
  // add unicast incoming port
  FD_SET(s, fds);
  
  // add unicast connections
  for (int i = 0; i < MAX_MEMBERS; i++) 
    if (browselist[i].socket > 0) 
      FD_SET(browselist[i].socket, fds);
}

/* Function to set up multicast socket
 *
 * Returns the set up socket
 */
int setup_multicast() {
  int sd;
  struct ip_mreq group;

  // Initialize socket
  sd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sd < 0) {
    perror("Error opening datagram socket");
    exit(1);
  }
 
  // Don't block the local multicast port
  int reuse = 1;
  if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
    perror("Error setting SO_REUSEADDR");
    exit(1);
  }

  // Prevent receiving own multicast messages
  char loopch = 0;
  if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopch, sizeof(loopch)) < 0) {
    perror("Error setting IP_MULTICAST_LOOP");
    close(sd);
    exit(1);
  }

  // Set multicast group address and port
  memset((char* ) &msock, 0, sizeof(msock));
  msock.sin_family = AF_INET;
  msock.sin_port = htons(MC_GROUP_PORT);
  msock.sin_addr.s_addr = inet_addr(MC_GROUP_ADDR);

  if(bind(sd, (struct sockaddr*)&msock, sizeof(msock))) {
    perror("Error binding datagram socket");
    exit(1);
  }

  // Joining multicast group
  group.imr_multiaddr.s_addr = inet_addr(MC_GROUP_ADDR);
  group.imr_interface.s_addr = INADDR_ANY;
  if(setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0) {
     perror("Error joining multicast");
     exit(1);
   }

  pdebug(" Multicast set up\n");
  
  // Return set up socket
  return sd;
}

/* Function to set up local listening, passive open unicast socket 
 *
 * Returns the set up socket
 */
int setup_unicast_listen() {
  int s;
  struct sockaddr_in srv;

  pdebug(" Setting up incoming socket\n");

  // Initialize socket
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("Error opening local listening socket");
    exit(1);
  }

  // Set port number and bind socket to port
  // Accept connections from any IP
  srv.sin_addr.s_addr = INADDR_ANY;
  srv.sin_port = htons(UC_DATA_PORT);
  srv.sin_family = AF_INET;
  if (bind(s, (struct sockaddr *)&srv, sizeof(srv)) < 0) {
    perror("Error binding local listening socket");
    exit(1);
  }

  // Set socket passive open and listen for up to 5 simultaneous commections
  if (listen(s,5) < 0) {
    perror("Error listening to local socket");
    exit(1);
  }

  // Return set up socket
  return s;
}

/* Function to set up unicast connections to other members
 *
 * Returns the number of opened connections
 * Returns -1 if there was an error
 */
int setup_unicast() {
  int newsock, returnvalue;
  struct sockaddr_in options;

  // Because TCP is full duplex, we only need one connection between two members
  // Every client opens connections to members that lower in the browse list
  // than itself. All higher clients will connect to said client.
  for (int i = 0; i < localindex; i++) {
    // Only connect if there is a socket to connect to
    if (browselist[i].socket == -1) { 
      pdebug(" opening connection to %s, i is %d\n", browselist[i].ip, i);
      
      // Initialize a new socket. If this fails, quit the program        
      newsock = socket(AF_INET, SOCK_STREAM, 0);
      if (newsock < 0) {
        perror("Error opening unicast socket, closing chat");
        close_chat();
      }

      // Set destination IP and port and connect
      options.sin_addr.s_addr = inet_addr(browselist[i].ip);
      options.sin_port = htons(UC_DATA_PORT);
      options.sin_family = AF_INET;
      if (connect(newsock, (struct sockaddr *)&options, sizeof(options)) < 0) {
        perror("Error connecting to unicast socket");
        return -1;
      }
      // Increment number of opened sockets
      returnvalue++;
      // Save socket in browse list
      browselist[i].socket = newsock;
    } 
  }
  
  // Return number of opened sockets
  return returnvalue; 
}

/* Function to send data over multicast
 *
 * Arguments
 * int type: Packet type as defined in header
 * char* data: Packet data, maximum length as defined in header
 *
 * Returns the return value of sendto()
 */
int send_multicast(int type, char* data) {
  packet packet;
  // Create raw data packet
  packet = create_packet(type, data);
  
  if ( data != NULL) {
    //                                              + 4 Bytes (header) 
    return sendto(sd, (char *)&packet, strlen(data) + 4, 0, (struct sockaddr*)&msock, sizeof(msock));
  }
  else
    //                                 4 Bytes (header)
    return sendto(sd, (char *)&packet, 4, 0, (struct sockaddr*)&msock, sizeof(msock));
}

/* Function to send data over unicast
 *
 * Arguments
 * int type: Packet type as defined in header
 * char* data: Packet data, maximum length as defined in header
 *
 * Returns the value of the last write()
 */
int send_unicast(int type, char* data) {
  packet packet;
  // Create raw data packet
  packet = create_packet(type, data);

  // Initialize return value
  int returnvalue;
  returnvalue = 0;

  if ( type == LEAVE_GROUP )
    // Only send LEAVE_GROUP to master, saved in the first browse list entry
    returnvalue = send(browselist[0].socket, (char *)&packet, MAX_MSG_LEN + 4, 0);
  else {
    // All other unicat packets go to every member
    for (int i = 0; i < MAX_MEMBERS; i++) {
      if ((browselist[i].socket > 0) // Dont send to empty sockets
          // Dont send to ourself
          && (strncmp(inet_ntoa(localip), browselist[i].ip, INET_ADDRSTRLEN) != 0 ) ) { 
        returnvalue = write(browselist[i].socket, (char *)&packet, MAX_MSG_LEN + 4);
          pdebug(" socket: %d\n", browselist[i].socket);
          pdebug(" sending data: %s\n", data);
      }
    }
  }

  return returnvalue;
}

/* Function to set new application state
 * Also updates the window's state output
 */
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

/* Function to set the global timer
 *
 * Arguments
 * int sec: Second value
 * int usec: Micro second value
 */
void setGlobalTimer(int sec, int usec) {
  globalTimer.tv_sec  = sec;
  globalTimer.tv_usec = usec;
}

/* Function to prepare data to be sent
 *
 * Arguments
 * int type: Packet type as specified in header
 * char* data: Packet data, maximum length as specified in header
 *
 * Returns a packet struct ready to be sent as specified in header
 */
packet create_packet(int type, char* data) {
  // Increments sequence number and returns to 1 if value is >255 
  seqno = seqno + 1 % 255; 

  packet packet;
  uint32_t header;
  uint16_t datalen;

  // Clean packet.data from any possible memory garbage
  memset(packet.data, 0, MAX_MSG_LEN);
  // Only set datalen if there is data
  if (data != NULL) { 
    datalen = strlen(data);
    strncpy(packet.data, data, datalen);
  } else
    datalen = 0;

  // Format the header into a bit field by bit shifting the integers
  // Headerformat
  // 32Bit = VERSION 2    TYPE 5      OPTIONS 1  SEQNO 8      DATALEN 16 
  header = (0x01 << 30)|(type << 25)|(0 << 24)|(seqno << 16)|(datalen);
  // Transform bit field into network byte order for safe sending
  packet.header = htonl(header);

  // Return packet struct, ready to be sent
  return packet;
}

/* Function to decode raw packet into local_packet struct
 *
 * Argument
 * packet: Raw received packet
 *
 * Returns a local_packet struct filled with received data
 */
local_packet receive_packet(packet packet) {
  local_packet local_packet;
  uint32_t header;

  // Convert header bit field back into local byte order
  header = ntohl(packet.header);
  // Bit shift and mask bit field to retrieve original integers
  local_packet.version = (header >> 30) & 3;
  local_packet.type = (header >> 25) & 31;
  local_packet.options = (header >> 24) & 1;
  local_packet.seqno = (header >> 16) & 255;
  local_packet.datalen = header & 65535;

  // Eliminate any memory garbage from local_packet struct
  memset(local_packet.data, 0, strlen(local_packet.data));

  // Only copy data if there is data present
  if (local_packet.datalen != 0) {
    strncpy(local_packet.data, packet.data, sizeof(packet.data));
  }

  // Return local_packet struct
  return local_packet;
}

/* Function to add an entry to a specific browse list position
 *
 * Arguments
 * char* clientip: IP of the client
 * int i: Position in the browse list
 */
void addToBrowseList(char* clientip, int i) {
  pdebug(" adding item to browse list\n");
  int duplicate = 0;

  // Check if item already in browselist
  for (int index = 0; index < i + 1; index++) {
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
    // Check if clientip is our local ip
    if (strncmp(clientip, inet_ntoa(localip), INET_ADDRSTRLEN) == 0) {
      // Local index in browselist
      localindex = i; 
    }
    // Copy client ip into given browse list entry
    strncpy(browselist[i].ip, clientip, INET_ADDRSTRLEN);

    // Get the client's host name
    hostent* host;
    in_addr ip;
    ip.s_addr = inet_addr(clientip);
    host = gethostbyaddr((char*)&ip, sizeof(ip), AF_INET);
    strncpy(browselist[i].name, host->h_name, strlen(host->h_name));
    
    // Initialize socket value
    browselist[i].socket = -1;

    // Display a join message if client isn't us
    if ( i != localindex) 
      poutput(" >>> %s has entered the building\n", browselist[i].name);
  }
}

/* Function to append a client to the browse list
 *
 * Arguments
 * char* clientip: IP of client to be added
 *
 * Returns the browse list length after appending the new client to it
 * Returns -1 client is already in list
 */
int addToBrowseList(char* clientip) {
  pdebug(" appending item to browse list\n");

  // Check if client is already in the browse list
  for (int index = 0; index < browselistlength - 1; index++) {
    if (!strcmp(browselist[index].ip,clientip)) {
      return -1;
    }
  }

  // Check if client is us
  int i = browselistlength;
  if (strncmp(clientip, inet_ntoa(localip), INET_ADDRSTRLEN) == 0) {
    // Local index in browselist
    localindex = i; 
  }
  strncpy(browselist[i].ip, clientip, INET_ADDRSTRLEN);

  // Get host name
  hostent* host;
  in_addr ip;
  ip.s_addr = inet_addr(clientip);
  host = gethostbyaddr((char*)&ip, sizeof(ip), AF_INET);
  strncpy(browselist[i].name, host->h_name, strlen(host->h_name));
  
  // Initialize socket value
  browselist[i].socket = -1;

  // Display join message
  if ( i != localindex) 
    poutput(" >>> %s has entered the building\n", browselist[i].name);

  // Return new browse list length
  i++;
  return i;
}

/* Function to remove a client from the browse list
 *
 * Arguments
 * int i: Entry in the browse list
 *
 * Returns the new browse list length
 */
int removeFromBrowseList(int i) {
  pdebug(" closing socket %d\n", browselist[i].socket);
  
  // Close the socket
  close(browselist[i].socket); 
 
  // If entry was the last one, just initialize it's values 
  if ( i == browselistlength - 1) {
    browselist[i].socket = -1;
    memset(browselist[i].name, 0, 1024);
    memset(browselist[i].ip, 0, INET_ADDRSTRLEN);
  } 
  // Otherwise move all remaining entries one down
  else {
    for (int index = i; index < browselistlength - 1; index++) {
      strncpy(browselist[i].name, browselist[i+1].name, 1024);
      strncpy(browselist[i].ip, browselist[i+1].ip, INET_ADDRSTRLEN);
      browselist[i].socket = browselist[i+1].socket;
    }
  }

  // Return new browse list length
  int newbllength;
  newbllength = browselistlength - 1;
  pdebug(" new browselist length: %d\n", newbllength);
  return newbllength;
}

// Function to reset the BrowseList
void reset_browselist() {
  for (int i = 0; i < MAX_MEMBERS - 1; i++) {
    // Close the socket
    if (browselist[i].socket > 0)
      close(browselist[i].socket);

    // Initialize the entry's values
    browselist[i].socket = -1;
    memset(browselist[i].name, 0, 1024);
    memset(browselist[i].ip, 0, INET_ADDRSTRLEN);
  }

  // Set browse list length to 0
  browselistlength = 0;
}

/* Function to send a browse list entry via multicast
 *
 * Arguments
 * int index: Index to be sent
 */
void send_BrowseListItem(int index) {
  // Format the data as comma separated values as follows
  // index, browselistlength, length of IP, IP in dotted notation
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

  // Send the data via multicast
  send_multicast(BROWSE_LIST, data);
}

/* Function to receive and handle browse list entries
 *
 * Arguments
 * char* data: Browse list data as comma separated values
 *
 * Returns the new browse list length
 */
int receive_BrowseListItem(char* data) {
  char bllength[16];
  char blindex[16];
  char iplength[16];
  char ip[16];

  // strtok() cuts data into tokens, separated by the given character 
  strncpy(bllength, strtok(data, ","), 16);
  strncpy(blindex, strtok(NULL, ","), 16);
  strncpy(iplength, strtok(NULL, ","), 16);
  strncpy(ip, strtok(NULL, ","), 16);

  // Add received browse list entry to browse list
  addToBrowseList(ip, atoi(blindex));

  // Return received browse list length
  return atoi(bllength);
}

// Exit function
void close_chat() {
  poutput(" >> quitting chat\n");

  // Tell the group that we are leaving
  if (appl_state == I_AM_MASTER)
    send_multicast(LEAVE_GROUP_MASTER, NULL);
  else
    send_unicast(LEAVE_GROUP, NULL);

  // Close multicast socket
  close(sd);
  // Close incoming unicast socket
  close(s);

  // Close allpossible unicastsockets
  for (int i = 3; i < FD_SETSIZE; i++ )
    close(i);

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

// Function to display the current browse list
void display_browselist() {
  poutput(" >>> Currently in this building:\n");
  poutput("    [M]\t%s\n", browselist[0].name);
  for (int i = 1; i < browselistlength; i++)
    poutput("\t%s\n", browselist[i].name);
}

// Function to display help
void display_help() {
  poutput(" >>> Available Commands:\n");
  poutput("\t/quit - Leave the building\n");
  poutput("\t/who  - Display who is inside the building\n");
  poutput("\t/help - Display this help\n");
}


/* Wow - you have read through nearly 1230 lines of code. Respect!
 *
 * We do assure you that no animals were harmed while programming this tool.
 * A big thanks to Mountain Dew and Lucky Strike, without you
 * this project would never have been possible.
 *
 * Keep it safe, 42 and godspeed!
 */
