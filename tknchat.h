#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <syslog.h>
#include <cstring>
#include <ifaddrs.h>
#include <netdb.h>
#include <ncurses.h>
#include <pthread.h>

#define MC_GROUP_ADDR "225.2.0.1"
#define MC_GROUP_PORT 5020
#define UC_DATA_PORT  3850
#define MAX_MSG_LEN   1492  // Ethernet frame 1500 - 8bytes UDP header
#define MASTER_REFRESH_INTERVAL   60  // 60 sec. master refresh interval
#define MAX_MEMBERS               32

#define STATE_NULL            0x00
#define STATE_INIT            0x01
#define STATE_FORCE_ELECTION  0x02
#define STATE_NO_MASTER       0x03
#define STATE_I_AM_MASTER     0x04
#define STATE_ELECTION        0x05
#define STATE_MASTER_FOUND    0x06
#define STATE_IDLE            0x07
#define STATE_BROWSELIST_RCVD 0x08

#define CTRL_PKT            0x01
#define REQ_MEMBERSHIP      0x02
#define SEARCHING_MASTER    0x02
#define MASTER_LEVEL        0x03
#define I_AM_MASTER         0x04
#define FORCE_ELECTION      0x05
#define DATA_PKT            0x06
#define LEAVE_GROUP         0x07
#define LEAVE_GROUP_MASTER  0x08
#define CHECK_MASTER        0x09
#define GET_MEMBER_INFO     0x10
#define SET_MEMBER_INFO     0x11
#define GET_BROWSE_LIST     0x12
#define BROWSE_LIST         0x13

struct packet {
  uint32_t header; 
  char data[65536];
};

struct local_packet {
  int version;
  int type;
  int options;
  int datalen;
  int seqno;
  char data[65536];
};
/**/
struct sockaddr_in msock;
int sd; // datagram socket
int s; // unicast incoming

in_addr getIP(const char*);
int init_fdSet(fd_set*);
int setup_multicast();
int setup_unicast_listen();
int setup_unicast();
int send_multicast(int, char*);
void parse_options(int, char**);
void setNewState(int);
void setGlobalTimer(int, int);
packet create_packet(int, char*);
local_packet receive_packet(packet);
void addToBrowseList(char*, int);
int addToBrowseList(char*);
int removeFromBrowseList(int);
void reset_browselist();
int send_BrowseListItem(int);
int receive_BrowseListItem(char*);
int send_unicast(int, char*);
void close_chat();
WINDOW *create_newwin(int height, int width, int ystart, int xstart, int border);
void destroy_win(WINDOW *local_win);

void pdebug(const char* fmt, ...);
void poutput(const char* fmt, ...);

void *get_input(void *arg);

static struct option long_options[] = { 
  { "help",       0, NULL, 'h' }, 
  { "version",    0, NULL, 'v' }, 
  { "interface",  1, NULL, 'i' }, 
  { NULL,         0, NULL,  0  } 
}; 

struct BrowseList {
  char name[1024];
  char ip[INET_ADDRSTRLEN];
  int socket;
} browselist [MAX_MEMBERS];
