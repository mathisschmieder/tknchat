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

#define MC_GROUP_ADDR "225.2.0.1"
#define MC_GROUP_PORT 5020
#define UC_DATA_PORT  3850
#define MAX_MSG_LEN   1492  // Ethernet frame 1500 - 8bytes UDP header
#define MASTER_REFRESH_INTERVAL   60  // 60 sec. master refresh interval
#define MAX_MEMBERS               32

#define STATE_INIT            0x01
#define STATE_FORCE_ELECTION  0x02
#define STATE_NO_MASTER       0x03
#define STATE_I_AM_MASTER     0x04
#define STATE_ELECTION        0x05
#define STATE_MASTER_FOUND    0x06
#define STATE_IDLE            0x07
#define STATE_BROWSELIST_RCVD 0x08

// deprecated - dont use these
#define MC_REQUEST_MEMBERSHIP     0x01
#define MC_FORCE_ELECTION         0x02
#define MC_OS_LEVEL               0x03
#define MC_I_AM_MASTER            0x04
#define MC_GET_BROWSELIST         0x05
#define MC_CLIENTCREDENTIALS      0x06
#define MC_BROWSELIST             0x07
#define MC_GET_CLIENTCREDENTIALS  0x08

#define CTRL_PKT            0x01
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

struct ClientCredentials { //deprecated - dont use this
	char name[1024];
	sockaddr_in * sockaddr;
};

struct mc_packet { //deprecated - dont use this;
  int type;
  int OS_Level;
  struct ClientCredentials;
  //whole browselist (linked list)
};

struct sockaddr_in msock;
int sd; // datagram socket

sockaddr_in * getIP(const char*);
int init_fdSet(fd_set*);
int setup_multicast();
int send_multicast(int, char*);
void parse_options(int, char**);
void setNewState(int);
int getState();
void setGlobalTimer(int, int);
void pdebug(const char*);
packet create_packet(int, char*);
local_packet receive_packet(packet);

static struct option long_options[] = { 
  { "help",       0, NULL, 'h' }, 
  { "version",    0, NULL, 'v' }, 
  { "interface",  1, NULL, 'i' }, 
  { "nick",       1, NULL, 'n' }, 
  { NULL,         0, NULL,  0  } 
}; 

struct BrowseListItem {
  ClientCredentials member;
  BrowseListItem *next;
};

struct BrowseList {
  BrowseListItem *head;
};
