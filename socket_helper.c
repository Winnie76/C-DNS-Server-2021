/* most of the code is from practical 9 MAST30023 Computer System from the University of Melbourne */
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include "socket_helper.h"
#include "log_helper.h"


int func_client_socket(char* up_dns_hostname, char* up_dns_port)
{
    int sockfd, s;
    struct addrinfo hints, *serverinfo, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    s = getaddrinfo(up_dns_hostname, up_dns_port, &hints, &serverinfo);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    for (rp = serverinfo; rp != NULL; rp = rp->ai_next)
    {
        //create a socket for client itself
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1)
        {
            continue;
        }

        //make connection
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            break;//success
        }
        close(sockfd);
    }
    if (rp == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(serverinfo);
    return sockfd;
}


int func_server_socket(){
    int sockfd, enable, s;
    struct addrinfo hints, *res;

    //create address we listen on with provided port number
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    s = getaddrinfo(NULL, PORT_NUM, &hints, &res); //getaddrinfo(const char *node, e.g. "www.example.com"
                                                      //or IP const char *service,  // e.g. "http" or port number
                                                      //const struct addrinfo *hints, struct addrinfo **res);
                                                      //give this function three input parameters, and it gives you a pointer to a linked-list, res, of results.
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    //reuse port if possible
    //socket that was connected is still hanging around in the kernel
    //either wait for it to clear (a minute or so), or add code to your program allowing it to reuse the port
    enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }

    //bind ip address to socket (sock id, ip address, ip length)
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res);

    //listen on socket and ready to accept connections
    //incoming connection requests will be queued, max queue length is 5
    //automatic block state
    if (listen(sockfd, 5) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

