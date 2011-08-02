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

struct ClientCredentials {
	char name[1024];
	sockaddr_in * sockaddr;
};
struct sockaddr_in msock;
int sd; // datagram socket

sockaddr_in * getIP(const char*);
int init_fdSet(fd_set*);
int setup_multicast();
int send_multicast(char*);
