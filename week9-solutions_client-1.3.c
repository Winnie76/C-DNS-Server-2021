/* Client for 1.2 */
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <unistd.h>

void process_download(const int sockfd, const char *file_name);
int setup_client_socket(const int port, const char *server_name,
						struct sockaddr_in *serv_addr);
void process_upload(const int sockfd, const char *file_name);

int main(int argc, char *argv[])
{
	struct sockaddr_in serv_addr;
	char *server;
	int port;
	int sockfd;
	char buffer[256];

	if (argc < 3)
	{
		fprintf(stderr, "usage: %s hostname port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	port = atoi(argv[2]); //atoi converts a string into an integer
	server = argv[1];

	while (1)
	{
		/* Make connection */
		sockfd = setup_client_socket(port, server, &serv_addr); //reads (int port, char* server, sockaddr_in* serv_addr)
		// connect to a remote host
		// serv_addr containing the destination port and IP address
		// addrlen is the length in bytes of the server address
		if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		{
			perror("connect");
			exit(EXIT_FAILURE);
		}

		// input from stdin and buffer stores it
		/* Note: fgets stores \0 */
		printf("Enter command: ");
		if (!fgets(buffer, 255, stdin))
		{
			break;
		}
		strtok(buffer, "\n");

		/* Branch off depending on given input */
		// strncmp ( const char * str1, const char * str2, size_t num );
		// Compare characters of two strings. Compares up to num characters of the C string str1 to those of the C string str2.
		// if Return value < 0 then it indicates str1 is less than str2.
		if (strncmp(buffer, "DOWNLOAD", 8) == 0) //is buffer 'DOWNLOAD'
		{
			if (!(buffer + 9))
			{
				fprintf(stderr, "No file name has been given\n");
				close(sockfd);
				continue;
			}
			process_download(sockfd, buffer + 9);
		}
		else if (strncmp(buffer, "UPLOAD", 6) == 0) //is buffer 'UPLOAD'
		{
			if (!(buffer + 7))
			{
				fprintf(stderr, "No file name has been given\n");
				close(sockfd);
				continue;
			}
			process_upload(sockfd, buffer + 7);
		}
		else
		{
			fprintf(stderr, "Invalid command\n");
		}

		/* Close to let server know that we've finished sending our message */
		close(sockfd);
	}
}

/* Handles DOWNLOAD command */
void process_download(const int sockfd, const char *file_name)
{
	int filefd;
	char buffer[2048];
	int n;

	/* Send download command to server */
	sprintf(buffer, "DOWNLOAD %s", file_name); //sends formatted output to a string pointed to (put in buffer)
	n = write(sockfd, buffer, strlen(buffer)); //Writes cnt bytes from buf to the file or socket associated with fd
	if (n < 0)								   //return Number of bytes written on success
	{
		perror("write");
		exit(EXIT_FAILURE);
	}

	/* Read initial message */
	// From the file indicated by the file descriptor fd,
	// the read() function reads cnt bytes of input into the memory area indicated by buf.
	// 从 sockfd 读 2047 bytes 到 buffer 里
	n = read(sockfd, buffer, 2047);
	if (n < 0)
	{
		perror("read");
		exit(EXIT_FAILURE);
	}
	buffer[n] = '\0'; //告诉 结束了

	/* Take action depending on server response */
	// if buffer is 'NOT-FOUND'
	if (strncmp(buffer, "NOT-FOUND", 9) == 0) //compare up to 9
	{
		fprintf(stderr, "Server could not find %s\n", file_name);
		return;
	}
	// if buffer is 'OK'
	else if (strncmp(buffer, "OK ", 3) == 0)
	{
		/* Open file for writing */
		filefd = open(file_name, O_WRONLY | O_CREAT, 0600); //open file_name for writing or create file

		/* Write initial buffer contents */	  //(int fd, void* buf, size_t cnt)
		n = write(filefd, buffer + 3, n - 3); //Writes cnt bytes from buf to the file or socket associated with fd.
		if (n < 0)
		{
			perror("write");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		fprintf(stderr, "Wrong response from server, ignoring\n");
		return;
	}

	// Read from socket until whole file is received
	while (1)
	{
		n = read(sockfd, buffer, 2048);
		if (n == 0)
		{
			break;
		}
		if (n < 0)
		{
			perror("read");
			exit(EXIT_FAILURE);
		}

		n = write(filefd, buffer, n);
		if (n < 0)
		{
			perror("write");
			exit(EXIT_FAILURE);
		}
	}

	printf("Received file %s\n", file_name);
}

/* Handles UPLOAD command */
void process_upload(const int sockfd, const char *file_name)
{
	int filefd;
	int n, re;
	char buffer[2048];

	if (access(file_name, F_OK) != -1)
	{
		/* Open file */
		filefd = open(file_name, O_RDONLY);
		if (!filefd)
		{
			perror("open");
			exit(EXIT_FAILURE);
		}

		/* Write "UPLOAD " and file name */
		n = sprintf(buffer, "UPLOAD %s ", file_name);
		n = write(sockfd, buffer, n);
		if (n < 0)
		{
			perror("write");
			exit(EXIT_FAILURE);
		}

		/* Send file contents */
		re = 0;
		do
		{
			re = sendfile(sockfd, filefd, NULL, 2048); //copies 2048 data between one file descriptor and another
		} while (re > 0);							   //re: number of bytes written to out_fd is returned
		if (re < 0)
		{
			perror("ERROR sending file");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		fprintf(stderr, "File not found\n");
	}
}

/* Create and return a socket bound to the given port and server */
int setup_client_socket(const int port, const char *server_name,
						struct sockaddr_in *serv_addr)
{
	int sockfd;
	struct hostent *server;

	server = gethostbyname(server_name);
	if (!server)
	{
		fprintf(stderr, "ERROR, no such host\n");
		exit(EXIT_FAILURE);
	}
	bzero((char *)serv_addr, sizeof(serv_addr)); //bzero set all the socket structures with null values
	serv_addr->sin_family = AF_INET;
	bcopy(server->h_addr_list[0], (char *)&serv_addr->sin_addr.s_addr,
		  server->h_length);		   //copies server->h_length bytes from server->h_addr_list[0] to (char *)&serv_addr->sin_addr.s_addr
	serv_addr->sin_port = htons(port); // converts the unsigned integer hostlong from host byte order to network byte order.

	/* Create socket */
	// int socket(int domain, int type, int protocol)
	sockfd = socket(PF_INET, SOCK_STREAM, 0); // returns to you a socket descriptor that you can use in later system calls, or -1 on error.
	if (sockfd < 0)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	return sockfd;
}
