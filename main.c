#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>

char PORT_NUM[] = "8053";

int func_client_socket();
int main(int argc, char *argv[])
{
    //0.打开文件 dns_svr.log 用write

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
    //哪个socket在听， 在等别人的connection,如果没人连接就一直等--》自动block state直到有连接
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
        client_addr_size = sizeof(client_addr);
        //socket和socket之间的connection id就是 newsockfd
        newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (newsockfd < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        //0.打开文件 dns_svr.log 用write

        //1.读取开头2个byte--建立的连接是和dig这个指令，读他发的文件--dig是模拟客户端向你发送一个指令
        /* Read from client */
        n = read(newsockfd, ch, 2); // 从 newsockfd（connection id） 读 2 bytes 到 ch 里
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
        //2.16转成10进制
        length = ch[0] * 256 + ch[1];
        //length = ch[0] << 8;
        //length += ch[1];

        //3.根据刚刚读的length读取剩下的packet的内容，存到一个malloc的array中
        unsigned char *msg_pkt = malloc(length);
        assert(msg_pkt);
        int i = 0;
        while (length)
        {
            if (read(newsockfd, msg_pkt + i, 1)) //i=0把byte存在array第0位...
            {                                    //read 1 byte every time
                length--;
                i++;
            }
        }
        //不推荐这个，因为有时候消息断断续续不一定一次性能读完read(newsockfd, msg_pkt, length);

        //4.判断是不是AAAA以及按格式log这个request到dns_svr.log文件中
        //从index12开始，12代表第一个section长度（例子里是01代表长度1，后面跟着section内容是31，也就是数字1.
        //09代表第二个section长度，后面跟了9个东西，直到读到00也就是结束
        //每收到一个packet都需要在文件里log写--格式在spec里
        //需要已知file pointer对应一个文件被打开，解析域名需要知道msg_packet
        //第一个写timestamp
        time_t rawtime;
        struct tm *info;
        char buffer[80]; //保存时间的array
        time(&rawtime);  //给它raw time地址
        info = localtime(&rawtime);
        strftime(buffer, 80, "%FT%T%z", info); //把当前电脑的时间以string的形式用spec要的format写在buffer中
        fprintf(fp, "%s requested", buffer);   //用fprintf写时间到文件里

        //如何得到domain
        //以下代码不可以放在一起，要思考怎么连一起（应该是function里的）
        //从packet中提取domain，把domain保存在char array中-》array 叫 domain_buffer，大小随便，如256
        //（不用malloc如果在一个function 里，且不用return。只要普通array即可）
        //要知道msg_pkt
        //要知道有几个section
        int count = find_num_section(); //为了下面的.加进去--》怎么写domain一定在index12开始，先读index12的数字，比如是1，那个下一个section在index14开始，告诉长度9，因此下一个section在index24开始，此时发现byte为0，意味section结束了。因此看section跳到多少为0就可知
        int section_len;
        int i, n = 0;
        int start_index = 12; //domain是从index 12 才开始是第一个section长度，00为null对应所有section结束，由此知道多少个section
        //去每个section提取内容保存在domain的array里
        while (count)
        {
            section_len = msg_pkt[start_index]; //知道此section长度了
            for (i = 0; i < section_len; i++)
            {
                domain_buffer[n] = msg_pkt[start_index + i + 1];
                n++;
            }
            if (count != 1)
            {
                domain_buffer[n] = '.'; //用于1.comp30023的.
                n++;
            }
            start_index += section_len + 1;
            count--;
        }
        domain_buffer[n] = '\0'; //变成完整的string

        //5.1 如果是AAAA，发给upstream server
        //在00null后面一定是type，所以
        // if(null后面第一个byte==0 && null后面第二个byte==28){
        //      是AAAA
        // }else{不是}

        //要发完整的packet，client怎么发给我，我就怎么发给upstream server--之前先读了2 byte，再rest--要还原成完整packet
        //4. 如何发送并读取upstream 信息（用write发送）
        //前提：建立connection，要知道connection id 以及要发送的packet（new_pkt), 以及这个packet长度
        //发pkt
        write(connection_id, new_pkt, new_pkt_len);

        //收upstream 的 pkt
        unsigned char len_buffer[2];
        //先读前两个byte;读取upstream server是这里
        read(connection_id, len_buffer, 2); //先读两个到len_buffer去
        //把len_buffer转化为10进制的len

        //再读剩下的部分（读到res_pkt去）

        //client 连接在读完/接受完就关闭
        close(connection_id);
        //6. 把length header加到msg_pkt前面，生成新的packet
        //思路：有两个array。一个length array有两个byte，后一个packet msg. 那就做一个新的array，比packet msg大2个各自。然后分别copy之前两个array内容到新array

        //7. DNS_SVR作为client，连接上一级 upstream server
        //在一开始上课的client部分

        //8. 连接成功后，将完整的packet传给upstream server
        //在上面

        //9. log刚收到的upstream server的response
        void log_recv_res(FILE * fp, unsigned char *res_pkt)
        {
            //log timestamp-->上面说过

            //log domain--》也说过（从index12那里开始读）

            //log ipv6 address
            //假设此时start_index = NULL对应那个index
            //然后先判断是不是ipv6的答案， start index后+7就是对应type
            start_index += 7;
            if (res_pkt[start_index] + res_pkt[start_index + 1] != 28)
            {
                return;
            }
            else
            {                     //是AAAA-》读ipv6地址
                start_index += 8; //跳到00，通过00和10知道ipv6长度就可以打出来
                //ipv6的length
                int len = res_pkt[start_index] * 256;
                len += res_pkt[start_index + 1];
                start_index += 2;
                char buffer[256];
                //读取ip address用老师给的function inet_ntop --》用于把byte转成string 形式的ipv6
                inet_ntop(AF_INET6, res_pkt + start_index, buffer, buffer_size); //把ipv6读成string放在buffer的array里
                //                  从哪读                 保存转化好的string
                //log_ipv6
                fprintf(fp, "%s %s is at %s", time_buffer, domain_buffer, ipv6_buffer);
                //题外问题：写几行文件
                //收到request写一行；判断request是不是AAAA，不是的话写一行；是的话，发pkt给上一级server，收到server的response写一行。（每次收到pkt就写）
                //write是发送，read是存到某个array
            }
        }
        //10. 把length header加到res_pkt前面，生成新的pkt
        //和6一样自己想

        //11. send back to original client（就是把新pkt write回去）

        //5.2 如果不是AAAA则发回一个well formed DNS msg--作为response回，所以QR改成0，且R code改成4 发回去
        //log一个unimplemented request（request是每次收到packet request都要log的， 但如果不是AAAA就要多log一行unimplemented request）

        //12. 把length header加到msg_pkt前面，生成新的pkt
        //自己写

        //13. 修改pkt中QR和RCODE的值，发回给original client
        //把0000改成1000（第一位改成1--》response）
        int QR = 128;
        int RCODE = 4;
        pkt_msg[4] = pkt_msg[4] | QR;
        pkt_msg[5] = pkt_msg[5] | RCODE;
    }

    //14. 关闭DNS_SVR的socket
    //关闭socket以及connection
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