/* Server for 1.2 */
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <unistd.h>

void run_server(const int port);
void send_file(char *buffer, int sockfd);
void receive_file(char *buffer, int buffer_received, int sockfd);
int create_server_socket(const int port);

int main(int argc, char *argv[])
{
	int port;

	/* CL args */
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	port = atoi(argv[1]);

	run_server(port);

	return 0;
}

/* Server function stuff */
void run_server(int port)
{
	char buffer[2048];
	int sockfd, newsockfd;
	int n;

	sockfd = create_server_socket(port);

	/* Listen on socket, define max. number of queued requests */
	if (listen(sockfd, 5) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

	while (1)
	{
		/* Accept connection */
		newsockfd = accept(sockfd, NULL, NULL);
		if (newsockfd < 0)
		{
			perror("accept");
			exit(EXIT_FAILURE);
		}

		/* Read from client */
		n = read(newsockfd, buffer, 2047); // 从 newsockfd 读 2047 bytes 到 buffer 里
		if (n == 0)
		{
			close(newsockfd);
			continue;
		}
		if (n < 0)
		{
			perror("read");
			exit(EXIT_FAILURE);
		}
		buffer[n] = '\0';

		/* Branch off depending on command given by client */
		if (strncmp(buffer, "DOWNLOAD", 8) == 0)
		{
			send_file(buffer, newsockfd);
		}
		else if (strncmp(buffer, "UPLOAD", 6) == 0)
		{
			receive_file(buffer, n, newsockfd);
		}
		else
		{
			fprintf(stderr, "Unknown command, discarding\n");
		}

		close(newsockfd);
	}

	close(sockfd);
}

/* Handles UPLOAD */
void receive_file(char *buffer, int buffer_received, int sockfd)
{
	char *file_name;
	char *file_name_end;
	int filefd;
	int n;

	/* Pointer to start of file name */
	file_name = strstr(buffer, " ") + 1;
	if (!file_name)
	{
		fprintf(stderr, "No file name specified, ignoring");
		return;
	}

	/* Pointer to end of file name, set as null byte */
	file_name_end = strstr(file_name, " ");
	if (!file_name_end)
	{
		fprintf(stderr, "No file contents, ignoring");
		return;
	}
	file_name_end[0] = '\0';

	printf("UPLOAD request for file: %s\n", file_name);

	/* Open file */
	filefd = open(file_name, O_WRONLY | O_CREAT, 0600);

	/* Write first part of buffer to file */
	n = write(filefd, file_name_end + 1, buffer_received - strlen(buffer) - 1);

	/* Read and save rest of file */
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

	close(filefd);
}

/* Handles DOWNLOAD */
void send_file(char *buffer, int sockfd)
{
	char *file_name;
	int filefd;
	int re;
	int n;

	/* Extract file name */
	file_name = strstr(buffer, " ");
	if (!file_name)
	{
		n = write(sockfd, "NOT-FOUND", 9);
		if (n < 0)
		{
			perror("write");
			exit(EXIT_FAILURE);
		}
		return;
	}
	file_name += 1;

	printf("DOWNLOAD request for file: %s\n", file_name);
	if (access(file_name, F_OK) != -1)
	{
		/* Open file */
		filefd = open(file_name, O_RDONLY);
		if (!filefd)
		{
			perror("open");
			exit(EXIT_FAILURE);
		}

		/* Write "OK " */
		n = write(sockfd, "OK ", 3);
		if (n < 0)
		{
			perror("write");
			exit(EXIT_FAILURE);
		}

		/* Send file contents */
		re = 0;
		do
		{
			re = sendfile(sockfd, filefd, NULL, 2048);
		} while (re > 0);
		if (re < 0)
		{
			perror("ERROR sending file");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		n = write(sockfd, "NOT-FOUND", 9);
		if (n < 0)
		{
			perror("write");
			exit(EXIT_FAILURE);
		}
	}
}

/* Create and return a socket bound to the given port */
int create_server_socket(const int port)
{
	int sockfd;
	struct sockaddr_in serv_addr;

	/* Create socket */
	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	/* Create listen address for given port number (in network byte order)
	for all IP addresses of this machine */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	/* Reuse port if possible */
	int re = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0)
	{
		perror("Could not reopen socket");
		exit(EXIT_FAILURE);
	}

	/* Bind address to socket */
	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}

	return sockfd;
}
