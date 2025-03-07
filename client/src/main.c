//
//  main.c
//  client
//
//  Created by Ali Mahouk on 12/8/17.
//  Copyright © 2017 Ali Mahouk. All rights reserved.
//

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DP_PORT "1992"
#define DP_PROTO_CLHEAD	"DP 1.0"
#define MAXDATASIZE 128 // max number of bytes we can get at once.

void *get_in_addr(const struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, const char *argv[])
{
	struct addrinfo *info;
	struct addrinfo *p_info;
	struct addrinfo hints;
	char buffer[MAXDATASIZE];
	char addr_str[INET6_ADDRSTRLEN];
	ssize_t len_bytes;
	int addr_result;
	int sockfd;
	
	sockfd = -1;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	if ((addr_result = getaddrinfo(argv[1], DP_PORT, &hints, &info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_result));
		return 1;
	}
	
	// Loop through all the results and connect to the first we can.
	for (p_info = info; p_info != NULL; p_info = p_info->ai_next) {
		if ( (sockfd = socket(p_info->ai_family, p_info->ai_socktype, p_info->ai_protocol)) == -1 ) {
			perror("client: socket\n");
			continue;
		}
		
		if (connect(sockfd, p_info->ai_addr, p_info->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect\n");
			continue;
		}
		
		break;
	}
	
	if (!p_info) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}
	
	inet_ntop(p_info->ai_family, get_in_addr((struct sockaddr *)p_info->ai_addr), addr_str, sizeof(addr_str));
	printf("client: connecting to %s\n", addr_str);
	freeaddrinfo(info);
	
	/*if ((len_bytes = recv(sockfd, buffer, MAXDATASIZE - 1, 0)) == -1) {
		perror("recv");
		exit(1);
	}
	
	buffer[bytelen] = '\0';
	
	printf("client: received '%s'\n", buffer);*/
	
	char *request = "DP 1\r\n-f test.txt\r\n-r foo@mahouk.co\r\n\r\n";
	
	if ((len_bytes = send(sockfd, request, strlen(request), 0)) == -1) {
		perror("send\n");
		exit(1);
	}
	
	close(sockfd);
	
	return 0;
}

