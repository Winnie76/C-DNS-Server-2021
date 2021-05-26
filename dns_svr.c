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
/* global variable */
char* PORT_NUM = "8053";


int main(int argc, char *argv[])
{
    int newsockfd, sockfd, two, length;
    unsigned char pkt_first_two[2];
    struct sockaddr_storage address_client;
    socklen_t client_address_size;
    FILE *fp;

    // open file in write mode
    // if file already exists, file content will be overwritten
    fp = fopen("dns_svr.log", "w"); 

    // unsatisfied argument amount
    if (argc < 3)
    {
        fprintf(stderr, "input less than 3\n");
        exit(EXIT_FAILURE);
    }

    // socket file description from server side socket
    sockfd = func_server_socket();

    // use while to keep accepting another query as soon as previous one has processed
    while (1)
    {
        // Accept connection between sockets
        client_address_size = sizeof(address_client);
        newsockfd = accept(sockfd, (struct sockaddr *)&address_client, &client_address_size);//newsockfd could be regarded as connection id
        if (newsockfd < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Read first 2 byte from client to get length of rest of pkt
        two = read(newsockfd, pkt_first_two, 2); 
        if (two == 0)
        {
            close(newsockfd);
            continue;
        }
        if (two < 0)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        //convert from hexadecimal to decimal
        length = pkt_first_two[0] * 256 + pkt_first_two[1];

        //read rest of pkt and store in an array
        unsigned char *packet_domain = malloc(length);
        assert(packet_domain);
        int k = 0;
        while (length)
        {
            if (read(newsockfd, packet_domain + k, 1)) 
            {                                    //read 1 byte every time
                length--;
                k++;
            }
        }
       

        //timestamp for dns_svr.log
        char buffer1[80]; //array that stores timestamp
        func_timestamp(buffer1);
        
        //domain name for dns_svr.log
        char domain_buffer1[256];
        int count_start_index = func_domain_name_buffer(domain_buffer1, packet_domain);

        //log request to dns_svr.log
        fprintf(fp, "%s requested %s\n", buffer1, domain_buffer1);  
        fflush(fp);
       
        //prepare the whole pkt for later sending to upstream server
        int new_length = pkt_first_two[0] * 256 + pkt_first_two[1] + 2;
        unsigned char *whole_packet = malloc(new_length);
        memcpy(whole_packet, pkt_first_two, 2);
        memcpy(whole_packet + 2, packet_domain, new_length - 2);

        // send to upstream server if request type is AAAA
        if(packet_domain[count_start_index + 1] + packet_domain[count_start_index + 2] == 28){
           
            int connection_id = func_client_socket(argv[1], argv[2]);
            write(connection_id, whole_packet, new_length);

            // receive pkt from upstream server

            // read first 2 bytes
            unsigned char first_two_buffer[2];
            read(connection_id, first_two_buffer, 2);
            // convert to decimal from hexadecimal
            int len_buffer_length = first_two_buffer[0] * 256 + first_two_buffer[1];
            int copy_len = first_two_buffer[0] * 256 + first_two_buffer[1];
            
            // read rest of pkt from upstream server
            unsigned char *res_pkt = malloc(len_buffer_length);
            int p = 0;
            while (copy_len)
            {
                if (read(connection_id, res_pkt + p, 1)) 
                {                                    //read 1 byte every time to avoid TCP error (msg doesn't come at the same time)
                    copy_len--;
                    p++;
                }
            }
            
            close(connection_id);
            
            // log response from upstream server
            func_log_res_pkt(fp, res_pkt);
        
            //prepare the whole pkt to send back to original client
            unsigned char *new_whole_pkt = malloc(len_buffer_length + 2);
            memcpy(new_whole_pkt, first_two_buffer, 2);
            memcpy(new_whole_pkt + 2, res_pkt, len_buffer_length);

            //send back new_whole_pkt to original client
            write(newsockfd, new_whole_pkt, len_buffer_length + 2);
        
        }else{
            // if not AAAA request then send back well formed DNS msg; log additional line -- umimplemented request
            char buffer_time[80]; //array that stores timestamp
           
            func_timestamp(buffer_time);
           
            fprintf(fp, "%s unimplemented request\n", buffer_time); 
            fflush(fp);

            //modify QR and RCODE and then send back to original client
            int QR = 128;
            int RCODE = 132;
            whole_packet[4] = whole_packet[4] | QR;
            whole_packet[5] = whole_packet[5] | RCODE;
            write(newsockfd, whole_packet, new_length);
        }
    }
    //close dnv_svr.log, socket and connection
    fclose(fp);
    close(sockfd);
    close(newsockfd);
    return 0;
}

