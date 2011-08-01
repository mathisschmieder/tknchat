#include <stdio.h>      
#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>

int main (int argc, const char * argv[]) {
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * if_addr=NULL;

    const char* eth = "eth0";

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if ((ifa ->ifa_addr->sa_family==AF_INET) && (strcmp(ifa->ifa_name, eth) == 0)) { 
            if_addr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
        } 
    }
    char addressBuffer[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, if_addr, addressBuffer, INET_ADDRSTRLEN);

    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);

    printf("IP: %s - Hostname: %s\n", addressBuffer, hostname);
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
    return 0;
}
