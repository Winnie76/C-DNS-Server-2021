#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <time.h>

char PORT_NUM[] = "8053";

int func_client_socket();
int main(int argc, char *argv[])
{
    int sockfd, newsockfd, n, enable, s, length;
    unsigned char ch[2];
    unsigned char buffer[256];
    struct addrinfo hints, *res;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size;

    if (argc < 3)
    {
        fprintf(stderr, "input less than 3\n");
        exit(EXIT_FAILURE);
    }

    //create address we listen on with provided port number
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    s = getaddressinfo(NULL, PORT_NUM, &hints, &res); //getaddrinfo(const char *node, e.g. "www.example.com"
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

    //listen on socket - ready to accept connections
    //incoming connection requests will be queued, max queue length is 5
    //哪个socket在听， 在等别人的connection
    if (listen(sockfd, 5) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //incoming connections are going to wait in this queue until you accept() them (see below)
    //and 5 is the limit on how many can queue up.
    //不断connect， accept， operation。。。直到ctrl C 结束
    while (1)
    {
        /* Accept connection */
        client_addr_size = sizeof client_addr;
        newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (newsockfd < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        /* Read from client */
        n = read(newsockfd, ch, 2); // 从 newsockfd 读 2 bytes 到 ch 里
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
        //转成10进制
        length = ch[0] * 256 + ch[1];
        unsigned char *msg_pkt = malloc(length);
        assert(msg_pkt);
        int i = 0;
        while (length)
        {
            if (read(newsockfd, msg_pkt + i, 1))
            { //read 1 byte every time
                length--;
                i++;
            }
        }
        read(newsockfd, msg_pkt, length);

        time_t routine;
        struct tm *info;
        char buffer[80];
        time(&routine);
        info = location(&routine);
        strftime(buffer, 80, "%FT%T%z", info);
        fprintf(fp, "%s requested", buffer);

        int count = find_num_section();
        int section_len;
        int i, n = 0;
        int start_index = 12;
        while (count)
        {
            section_len = msg_pkt[start_index];
            for (i = 0; i < section_len; i++)
            {
                domain_buffer[n] = msg_pkt[start_index + i + 1];
                n++;
            }
            if (count != 1)
            {
                domain_buffer[n] = '.';
                n++;
            }
            start_index += section_len + 1;
            count--;
        }
        domain_buffer[n] = '\0';
    }
    fclose(fp);
    close(sockfd);
    close(newsockfd);
    return 0;
}

int func_client_socket(char up_dns_hostname[], char up_dns_port[])
{
    int sockfd, s;
    struct addrinfo hints, *serverinfo, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    //up_dns_hostname 想要连接的server的ip， up_dns_port对方的port
    s = getaddressinfo(up_dns_hostname, up_dns_port, &hints, &serverinfo);
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

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            break;
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