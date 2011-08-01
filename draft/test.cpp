#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
int main(int argc, char *argv[]) {
  struct sockaddr_in sa;
  int s, n = sizeof(struct sockaddr), on = 1, len = strlen(argv[1]);
  if ( (s = socket(AF_INET,SOCK_DGRAM,0)) < 0 ) return(1);
  if ( setsockopt(s, SOL_SOCKET, SO_BROADCAST,  // broadcast option
                  (void *)&on, sizeof(on)) != 0) return(1);
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_BROADCAST); // 255.255.255.255
  sa.sin_port = htons(5020);
  sendto(s, argv[1], len, 0, (const struct sockaddr *)&sa, n);
}
