/*	copyright (c) 2011 jack. 
	
	simplified version of ftp client version 9.0
	
	client side.

	user command		ftp command
	-------------		--------------
	dir path/directory	-					(list local directory)
	ls  path/directory	LIST path/directory	(list remote directory)
	get path/filename	RETR path/filename
	put path/filename	STOR filename ntotal
	quit				-					(close the connection)

	NOTES:
	(1) get: can only get the files in default directory and its subdir.
	(2) put: can put any file to default directory only.
	(3) ls : can only list default directory and its subdir.
	(4) dir: can list any local directory.
	
	(*) the remote default directory is ./pub
*/

#include	"ftp.h"

int
main(int argc, char **argv)
{
	int					fd1, ntoken, data_port;
	struct sockaddr_in	addr1;
	char				buff[MAXLINE], command[MAXLINE], para[MAXLINE];

	if (argc != 2)
		err_quit("usage: ftpcli09 <server ip address>");
	
	if ( (fd1 = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		err_quit("socket() error");

	bzero(&addr1, sizeof(addr1));
	addr1.sin_family = AF_INET;
	addr1.sin_port = htons(CTRL_PORT);
	if (inet_pton(AF_INET, argv[1], &addr1.sin_addr) <= 0)
		err_quit("inet_pton() error");

	if (connect(fd1, (struct sockaddr *) &addr1, sizeof(addr1)) < 0)
		err_quit("connect() error (control connection)");

	printf("Waiting response from server ...\n");
	if (readline(fd1, buff, MAXLINE) > 0) // recv response from server
		fputs(buff, stdout);
	else
		err_quit("Waiting too long, terminated by peer\n");
	
	if (strncmp(buff, "-ERR", 4) == 0) {
		printf("Something wrong at the server!\n");
		exit(1);
	}

	do {
		printf("ftp>");
		if ((fgets(buff, MAXLINE, stdin) == NULL) ||
			(strlen(buff) == 1)) /* EOF (^D) or '\n' only */
			continue;
			
		ntoken = sscanf(buff, "%s%s", command, para);
		
		switch (ntoken) {
		case 2: 
			if (strcmp(command, "get") == 0)
				do_get(fd1, para);
			else if (strcmp(command, "put") == 0)
				do_put(fd1, para);
			else if (strcmp(command, "ls") == 0)
				do_ls(fd1, para);
			else if (strcmp(command, "dir") == 0)
				do_dir(para);
			else
				err_msg("invalid command(2).");
			break;
		case 1:
			if (strcmp(command, "ls") == 0)
				do_ls(fd1, ".");
			else if (strcmp(command, "dir") == 0)
				do_dir(".");
			else if (strcmp(command, "quit") == 0)
				goto end;
			else if ((strcmp(command, "get") == 0) ||
					 (strcmp(command, "put") == 0) )
				err_msg("file name not specified.");
			else 
				err_msg("invalid command(1).");
			break;
		default:  
			err_msg("invalid format(0)");
			break;
		}
	} while (1);

end:	
	printf("\n");
	close(fd1);
	exit(0);
}

void
do_get(int fd1, char *path)
{
	char sendline[MAXLINE], recvline[MAXLINE];;
	int filefd, n, nbytes = 0;
	int cflags = O_CREAT | O_WRONLY | O_TRUNC;
	mode_t cmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	char *file;
	int ntotal, nleft;
	
	snprintf(sendline, MAXLINE, "%s %s\n", "RETR", path);
	write(fd1, sendline, strlen(sendline)); // send command to server
	
	readline(fd1, recvline, MAXLINE); // recv response from server
	fputs(recvline, stdout);
	
	if (strncmp(recvline, "-ERR", 4) == 0) {
		return;
	}
	sscanf(recvline + strlen("+OK "), "%d", &ntotal);

	if ((file = strrchr(path, '/')) != NULL)
		file++;
	else
		file = path;

	if ( (filefd = open(file, cflags, cmode)) < 0) {
		err_msg("can't create this file");
		return;
	}
	
	printf("Total %d bytes, received: ", ntotal);
	nleft = ntotal;
	while ( nleft > 0 ){
		if ( (n = read(fd1, recvline, MAXLINE)) ==  0)
			err_quit("data connection terminated");
		
		nbytes += n;
		nleft -= n;
		write(filefd, recvline, n);
		n = printf("%d bytes.", nbytes);
		back(n);
	}
	printf("\n");

	close(filefd);
}	

void
do_put(int fd1, char *path)
{
	char sendline[MAXLINE], recvline[MAXLINE];;
	int filefd, n, nbytes = 0;
	char *file;
	struct stat ss;
	
	if ( (filefd = open(path, O_RDONLY, 0)) < 0) {
		/* use flag O_RDWR to prevent stor. directory */
		err_msg("can't put the file"); 
		return;
	}
	if ((file = strrchr(path, '/')) != NULL)
		file++;
	else
		file = path;

	fstat(filefd, &ss);

	snprintf(sendline, MAXLINE, "%s %s %ld\n", "STOR", file, ss.st_size);
	write(fd1, sendline, strlen(sendline)); // send command to server
	
	readline(fd1, recvline, MAXLINE); // recv response from server
	fputs(recvline, stdout);
	
	if (strncmp(recvline, "-ERR", 4) == 0) {
		close(filefd);
		return;
	}
	
	printf("Total %ld bytes, sent: ", ss.st_size);
	while ( (n = read(filefd, recvline, MAXLINE)) > 0) {
		nbytes += n;
		write(fd1, recvline, n);
		n = printf("%d bytes.", nbytes);
		back(n);
	}
	printf("\n");

	close(filefd);
}

void
do_ls(int fd1, char *para)
{
	char buff[MAXLINE], sendline[MAXLINE], recvline[MAXLINE];;
	int n;
	
	snprintf(sendline, MAXLINE, "%s %s\n", "LIST", para);
	write(fd1, sendline, strlen(sendline)); // send command to server
	
	readline(fd1, recvline, MAXLINE); // recv response from server
	fputs(recvline, stdout);
	
	if (strncmp(recvline, "-ERR", 4) == 0) {
		return;
	}
		
	while ( (n = readline(fd1, recvline, MAXLINE)) > 0) {
		if (strcmp(recvline, ".\r\n") == 0)
			break;
		write(1, recvline, n);
	}
}

void
do_dir(char *path)
{
	char buff[MAXLINE], abspath[MAXLINE];
	int i, n, m;
	struct dirent **namelist;
	struct stat ss;
	struct tm *ptrtm;
	
	realpath(path, abspath);
	/* m: number of entries */
	if ((m = scandir(abspath, &namelist, NULL, alphasort)) < 0) {
		err_msg("can't list the directory content");
		return;
	}

	printf("local directory:\n");
	printf(" %6s %10s %19s %s\n",
	"mode", "size", "modification time", "name");
	
	/* retrieve file information */
	n = 0; /* n: numer of entries, '.' and '..' excluded */
	for (i = 0; i < m; i++) {
		if ( (strcmp(namelist[i]->d_name, ".") == 0) ||
			 (strcmp(namelist[i]->d_name, "..") == 0))
			continue;
		snprintf(buff, MAXLINE, "%s/%s", abspath, namelist[i]->d_name);
		if (stat(buff, &ss) != 0)
			break;
		ptrtm = localtime(&ss.st_ctime);
		strftime(buff, sizeof(buff),  "%Y-%m-%d %H:%M:%S", ptrtm);
		if ( S_ISDIR(ss.st_mode))
			printf("d%6o %10d %19s %s\n", 
			ss.st_mode, (int)ss.st_size, buff, namelist[i]->d_name);
		else
			printf("-%6o %10d %19s %s\n", 
			ss.st_mode, (int)ss.st_size, buff, namelist[i]->d_name);
		n++;
		free(namelist[i]);
	}
	free(namelist);
	
	printf("%d entry(s) in total.\n", n);
}

void
back(int n)
{
	for ( ; n--; n >= 0)
		putchar(BS);
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

void
err_msg(const char *str)
{
	char msg[MAXLINE];

	snprintf(msg, MAXLINE, "===> Hey! something went wrong: %s\n", str); 
	fputs(msg, stderr);
	return;
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
