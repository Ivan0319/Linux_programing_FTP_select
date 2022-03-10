/*	copyright (c) 2011 jack. 
	
	ftp09.h
	
	concurrent, simplified version of ftp.
	
*/

#include <stdio.h> 
#include <string.h> 
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <limits.h>
#include <time.h>
#include <arpa/inet.h>

#define CTRL_PORT	4257
// #define DATA_PORT	4258
#define MAXLINE		1024
/* back space */
#define BS			8
/* 2nd para. for listen */
#define LISTEN_Q	5
/* server default directory */
#define DEFAULT_DIR	"./pub"

/* functions designed for client */

void do_get(int fd1, char *para);
void do_put(int fd1, char *para);
void do_ls(int fd1, char *para);
void do_dir(char *path); /* local directory */
void back(int n);

/* functions designed for server */
void do_RETR(int fd1, char *para);
void do_STOR(int fd1, char *para, int fsize);
void do_LIST(int fd1, char *para);
void do_ERR(int fd1, char *str);
void do_OK(int fd1, char *str);

void sig_chld(int signo);

/* utilities */
void err_quit(const char *str);
void err_msg(const char *str);
ssize_t readline(int fd, void *vptr, size_t maxlen);
