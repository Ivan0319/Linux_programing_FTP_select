/*	copyright (c) 2011 jack.

	simplified version of ftp server version 9.0
	concurrent server, serve many clients at a time
	and using ONE connection for each client in child server process
*/

#include	"ftp.h"

int
main(int argc, char **argv)
{
	int					listenfd1, connfd1, ntoken, pid;
	socklen_t			clilen;
	struct sockaddr_in	addr1, cliaddr;
	char				buff[MAXLINE], command[MAXLINE], para[MAXLINE];
	int	fsize;

	chdir(DEFAULT_DIR); /* default directory */
	
	/* for control connection */
	if ((listenfd1 = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		err_quit("socket() error");

	bzero(&addr1, sizeof(addr1));
	addr1.sin_family      = AF_INET;
	addr1.sin_addr.s_addr = htonl(INADDR_ANY);
	addr1.sin_port        = htons(CTRL_PORT);

	if (bind(listenfd1, (struct sockaddr *) &addr1, sizeof(addr1)) < 0)
		err_quit("bind() error");

	listen(listenfd1, LISTEN_Q);
	signal(SIGCHLD, sig_chld); /* must call waitpid() */
	
	for ( ; ; ) {
		clilen = sizeof(cliaddr);
		connfd1 = accept(listenfd1, (struct sockaddr *) &cliaddr, &clilen);

		printf("connection from %s, port %d, connfd1 is %d\n",
			   inet_ntop(AF_INET, &cliaddr.sin_addr, buff, sizeof(buff)),
			   ntohs(cliaddr.sin_port), connfd1);

		if ((pid = fork()) == 0) { /* child process */
			close(listenfd1); /* close listening socket */
		
			snprintf(buff, sizeof(buff), 
			"concurrent ftp server version 9.0 connected");
			do_OK(connfd1, buff); // +OK data_port sent to client
			
			/* 4read command from control connection */
			/* control connection will be closed by client */
			while (readline(connfd1, buff, MAXLINE) != 0) {
				printf("(child process (pid = %d), command from client: %s",
				getpid(), buff);
				ntoken = sscanf(buff, "%s%s%d", command, para, &fsize);
				switch (ntoken) {
				case 3:
					if (strcmp(command, "STOR") == 0)
						do_STOR(connfd1, para, fsize);
					break;
				case 2: 
					if (strcmp(command, "RETR") == 0)
						do_RETR(connfd1, para);
					else if (strcmp(command, "LIST") == 0)
						do_LIST(connfd1, para);
					else
						do_ERR(connfd1, "Unknown command parameter");
					break;
				default:
					do_ERR(connfd1, "Unknown command");
					break;
				}
			}
			close(connfd1); /* control connection */
			exit(0);
		}
		/* parent */
		printf("child %d forked!\n", pid);

		close(connfd1);	
	}
}

void
do_RETR(int connfd1, char *path)
{
	int		filefd;
	int		n;
	char	sendline[MAXLINE];
	struct stat ss;

	if ((strncmp(path, "/", 1) == 0) || (strstr(path, "..") != NULL)) {
		do_ERR(connfd1, "working in default directory ONLY!");
		return;
	}
	if ( (filefd = open(path, O_RDWR, 0)) < 0) { 
		/* use flag O_RDWR to prevent retr. directory */
		do_ERR(connfd1, "can't get the file");
		return;
	}
	fstat(filefd, &ss);
	snprintf(sendline, MAXLINE, "%ld", ss.st_size);
	/* open file succeeded: tell client OK */
	do_OK(connfd1, sendline);
	
	/* return file thru. data connection */
	while ( (n = read(filefd, sendline, MAXLINE)) > 0)
		write(connfd1, sendline, n);
	close(filefd);
}

void
do_STOR(int connfd1, char *para, int fsize)
{
	int		filefd;
	int		n;
	char	recvline[MAXLINE];
	int cflags = O_CREAT | O_WRONLY | O_TRUNC;
	mode_t cmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	char fname[MAXLINE];
	int nleft = fsize;

	if ( (filefd = open(para, cflags, cmode)) < 0) {
		/* error: must tell client */
		do_ERR(connfd1, "can't put the file");
		return;
	}
	/* create file succeeded: tell client OK */
	do_OK(connfd1, "the file is stored as you request");
	
	/* retrieve file thru. data connection */
	while ( nleft > 0) {
		if ( (n = read(connfd1, recvline, MAXLINE)) == 0)
			err_quit("Connection terminated");
		write(filefd, recvline, n);
		nleft -= n;
	}
	close(filefd);
}

void
do_LIST(int connfd1, char *path)
{
	char buff[MAXLINE], buff2[MAXLINE], abspath[MAXLINE];
	int i, n, m;
	struct dirent **namelist;
	struct stat ss;
	struct tm *ptrtm;

	if ((strncmp(path, "/", 1) == 0) || (strstr(path, "..") != NULL)) {
	/* can't list other directory */
		// printf("--->path=%s\n", path);
		do_ERR(connfd1, "working in default directory ONLY!");
		return;
	}
	
	realpath(path, abspath);
	/* m: number of entries */
	if ((m = scandir(abspath, &namelist, NULL, alphasort)) < 0) {
		do_ERR(connfd1, "can't list the directory content");
		return;
	}

	/* scandir succeeded: tell client OK */
	do_OK(connfd1, "remote directory:");
		
	snprintf(buff, MAXLINE, " %6s %10s %19s %s\n",
	"mode", "size", "modification time", "name");
	write(connfd1, buff, strlen(buff));
	
	/* retrieve file information */
	n = 0; /* n: numer of entries, '.' and '..' excluded */
	for (i = 0; i < m; i++) {
		if ( (strcmp(namelist[i]->d_name, ".") == 0) ||
			 (strcmp(namelist[i]->d_name, "..") == 0))
			continue;
		snprintf(buff, MAXLINE, "%s/%s", abspath, namelist[i]->d_name);
		if (stat(buff, &ss) != 0)
			return;
		ptrtm = localtime(&ss.st_ctime);
		strftime(buff2, sizeof(buff2),  "%Y-%m-%d %H:%M:%S", ptrtm);
		if ( S_ISDIR(ss.st_mode))
			snprintf(buff, MAXLINE, "d%6o %10ld %19s %s\n",
			ss.st_mode, ss.st_size, buff2, namelist[i]->d_name);
		else
			snprintf(buff, MAXLINE, "-%6o %10ld %19s %s\n",
			ss.st_mode, ss.st_size, buff2, namelist[i]->d_name);
		write(connfd1, buff, strlen(buff));
		n++;
		free(namelist[i]);
	}
	free(namelist);
	
	snprintf(buff, MAXLINE, "%d entry(s) in total.\n", n);
	write(connfd1, buff, strlen(buff));
		
	snprintf(buff, MAXLINE, ".\r\n");
	write(connfd1, buff, strlen(buff));

}

void
do_ERR(int connfd1, char *str)
{
	char msg[MAXLINE];
	
	snprintf(msg, MAXLINE, "-ERR %s\n", str);
	write(connfd1, msg, strlen(msg));
	
}

void
do_OK(int connfd1, char *str)
{
	char msg[MAXLINE];
	
	snprintf(msg, MAXLINE, "+OK %s\n", str);
	write(connfd1, msg, strlen(msg));
	
}

void
err_quit(const char *str)
{
	char msg[MAXLINE];

	snprintf(msg, MAXLINE, "ERROR! %s,(errno = %d, strerror = %s)\n", 
	str, errno, strerror(errno)); 
	fputs(msg, stderr);
	exit(1);
}

ssize_t
readline(int fd, void *vptr, size_t maxlen)
{
	ssize_t	n, rc;
	char	c, *ptr;

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
again:
		if ( (rc = read(fd, &c, 1)) == 1) {
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		} else if (rc == 0) {
			*ptr = 0;
			return(n - 1);	/* EOF, n - 1 bytes were read */
		} else {
			if (errno == EINTR)
				goto again;
			return(-1);		/* error, errno set by read() */
		}
	}

	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}

void
sig_chld(int signo)
{
	pid_t pid;
	int stat;

	while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0)
		printf("child %d terminated\n", pid);
	return;
}
