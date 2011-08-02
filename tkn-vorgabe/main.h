//----------------------------------------------------------------------
//  Technische Universität Berlin
//  Lehrstuhl für Telekommunikationsnetze
//
//  1231 L 303 Praktikum Kommunikationsnetze
//  Block D
//
//  Copyright 2001 Andreas Köpsel
//
//----------------------------------------------------------------------
//  Serverapplikation des CHAT-Programmes
//
//  main header file
//
 
#ifndef __CHATTOOL_
#define __CHATTOOL_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/time.h>
#include <assert.h>
#include <syslog.h>


//#define DEFAULT_MCADDR          "225.2.0.1"
#define DEFAULT_MCADDR          "224.0.0.1"
#define DEFAULT_MCPORT          5020
#define DEFAULT_DATA_PORT       3850
#define MAX_MSG_LEN             1492  // Ethernet frame 1500 - 8bytes UDP header
#define MASTER_REFRESH_INTERVAL   60  // 60 sec. master refresh interval 
#define MAX_MEMBERS               32

// states
#define STATE_INIT            0x01
#define STATE_FORCE_ELECTION  0x02
#define STATE_NO_MASTER       0x03
#define STATE_I_AM_MASTER     0x04
#define STATE_ELECTION        0x05
#define STATE_MASTER_FOUND    0x06
#define STATE_IDLE            0x07
#define STATE_BROWSELIST_RCVD 0x08


void abort_chat(int);
void db_dump(int);
void error_handler (long);
void parse_options(int, char**);
void version(char*);
void usage(char*);
int  find_highest_fd(void);
int  init_fdSet(fd_set*);

#define MAX_LOG_SIZE 400000
#define LOG_ALWAYS      0       /* special value for log severity level */ 



static struct option long_options[] = { 
                          { "daemon",              1, NULL, 'd' }, 
                          { "localport",           1, NULL, 'l' }, 
                          { "loglevel",            0, NULL, 'v' }, 
                          { "help",                0, NULL, 'h' }, 
                          { "version",             0, NULL, 'V' }, 
			  { "serveraddr",          1, NULL, 's' }, 
			  { "serverport",          1, NULL, 'p' }, 
                          { NULL,                  0, NULL,  0  } 
                  }; 


#endif
