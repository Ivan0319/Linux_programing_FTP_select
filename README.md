# Linux_programing_FTP_select
socket training use select()

	modify 2022 Ivan
	The child process will copies the memory space of the parent process more waste of resources when we use fork()
	Use I/O multiplexing function select(), there is no advantage over blocking IO, but that can monitor multiple IO ports at the same time
	to handlemultiple connect.

	copyright (c) 2011 jack.
	simplified version of ftp server version 9.0
	concurrent server, serve many clients at a time
	and using ONE connection for each client in child server process

	
	
