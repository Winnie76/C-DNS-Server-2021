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

extern char* PORT_NUM;

void func_log_res_pkt(FILE *fp, unsigned char *res_pkt);
void func_timestamp(char* time_buffer);
int func_domain_name_buffer(char* domain_name_buffer, unsigned char* packet_domain);