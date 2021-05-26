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

void func_log_res_pkt(FILE *fp, unsigned char *domain_name_pkt)
{
    // timestamp for dns_svr.log
    char time_buffer[80]; //store timestamp 
    func_timestamp(time_buffer);

    // domain name for dns_svr.log
    char domain_buffer[256];
    int count_start_index = func_domain_name_buffer(domain_buffer, domain_name_pkt);

    // index of null (end of all sections)
    int start_index = count_start_index;
    //index of response type
    start_index += 7;

    // is AAAA -> read address of ipv6
    if (domain_name_pkt[start_index] + domain_name_pkt[start_index + 1] == 28)
    {
        start_index += 8; //jump to index of length (by 00 and 10 to know the length of ipv6)
        start_index += 2; //jump to index of ipv6
        
        char ipv6_buffer[256];
        
        // read ip address --> convert from byte to string for ipv6
        inet_ntop(AF_INET6, domain_name_pkt + start_index, ipv6_buffer, 256); //read ipv6 as string and put in ipv6_buffer array
       
        fprintf(fp, "%s %s is at %s\n", time_buffer, domain_buffer, ipv6_buffer);
        fflush(fp);
    }
    // not AAAA -> do nothing
    else
    {                     
        return;
    }
}

// get the timestamp
void func_timestamp(char* time_buffer){
    struct tm *time_info;
    time_t address_rawtime;

    time(&address_rawtime);  //give it rawtime address
    time_info = localtime(&address_rawtime);
    strftime(time_buffer, 80, "%FT%T%z", time_info); //put timestamp in string format into time_buffer array

}

int func_domain_name_buffer(char* domain_name_buffer, unsigned char* packet_domain){
    int count = 0;
    int length_section;
    int j, m = 0;
    int start_index = 12; //index of length of the first section
    
    //find count -- number of sections
    int count_sec_len;
    int count_start_index = 12;
    while(1){
        count_sec_len = packet_domain[count_start_index];
        
        if(count_sec_len == 0){
            break;
        }
        count += 1;
        count_start_index += count_sec_len + 1;
    }
    
    //loop through all sections and store domain name in array
    while (count)
    {
        length_section = packet_domain[start_index]; //length of this section
        for (j = 0; j < length_section; j++)
        {
            domain_name_buffer[m] = packet_domain[start_index + j + 1];
            m++;
        }
        if (count != 1)
        {
            domain_name_buffer[m] = '.'; //for the dot in e.g 1.comp30023
            m++;
        }
        start_index += length_section + 1;
        count--;
    }
    domain_name_buffer[m] = '\0'; //complete string
    return count_start_index;
}