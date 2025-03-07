//
//  net.c
//  server
//
//  Created by Ali Mahouk on 12/8/17.
//  Copyright © 2017 Ali Mahouk. All rights reserved.
//

#include "net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include "protocol.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


/**********************
 * Private Prototypes 
 **********************/
void client_read(int);
void connection_log(const struct sockaddr_storage conn);
void grim_reaper(void);
void *in_addr_get(const struct sockaddr *);
void parcel_read(int, const struct data16 *);
void server_read(int);
void sigchld_handle(int);
int socket_is_local(const struct sockaddr *);
int socket_setup(const char *);
/**********************/


void client_read(int sockfd)
{
	char buffer[DP_PROTO_SERV_MAXREAD] = { 0 };
	struct token *request;
	ssize_t bytes_read;
	
	if ((bytes_read = read(sockfd, buffer, DP_PROTO_SERV_MAXREAD)) == -1)
		perror("read_client(1), read(3)");
	
	/***********
	 * PARSING
	 ***********/
	if (valid_check(buffer) == 1 &&
	    client_request_tokenise(buffer, bytes_read, &request) == 0) {
		client_request_parse(request);
		request_free(&request);
	} else {
		printf("read_client: error parsing client request!\n");
		
		if (valid_check(buffer) == 0)
			printf("Invalid request header.\n");
	}
}

/*
 * This function is too simple.
 */
void connection_log(const struct sockaddr_storage conn)
{
	char client_addr_str[INET6_ADDRSTRLEN];
	
	inet_ntop(conn.ss_family, in_addr_get((struct sockaddr *)&conn), client_addr_str, sizeof(client_addr_str));
	printf("LOG: connection from %s\n", client_addr_str);
}

int data64_send(const char *host, const struct data16 *head, const struct data64 *body)
{
	struct addrinfo *info;
	struct addrinfo *p_info;
	struct addrinfo hints;
	char addr_str[INET6_ADDRSTRLEN];
	ssize_t len_bytes;
	int addr_result;
	int sockfd;
	
	sockfd = -1;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	if ((addr_result = getaddrinfo(host, DP_PORT, &hints, &info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(addr_result));
		return 1;
	}
	
	// Loop through all the results and connect to the first we can.
	for (p_info = info; p_info != NULL; p_info = p_info->ai_next) {
		if ((sockfd = socket(p_info->ai_family, p_info->ai_socktype, p_info->ai_protocol)) == -1) {
			perror("send_data64(3), socket(3)");
			continue;
		}
		
		inet_ntop(p_info->ai_family, in_addr_get((struct sockaddr *)p_info->ai_addr), addr_str, sizeof(addr_str));
		printf("Connecting to host %s\n", addr_str);
		
		if ( connect(sockfd, p_info->ai_addr, p_info->ai_addrlen) == -1) {
			close(sockfd);
			perror("send_data64(3), connect(3)");
			continue;
		}
		
		break;
	}
	
	if (!p_info) {
		fprintf(stderr, "connect_server: failed to connect");
		return 2;
	}
	
	freeaddrinfo(info);
	
	if ((len_bytes = send(sockfd, head->bytes, head->len, 0)) == -1 ||
	    (len_bytes = send(sockfd, body->bytes, body->len, 0)) == -1) {
		perror("send_data64(3), send(4)");
		exit(1);
	}
	
	close(sockfd);
	
	return 0;
}

void grim_reaper(void)
{
	struct sigaction sigact;
	
	sigact.sa_handler = sigchld_handle;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_RESTART;
	
	if (sigaction(SIGCHLD, &sigact, NULL) == -1)
		perror("grim_reaper(0), sigaction(3)");
}

void *in_addr_get(const struct sockaddr *sockaddr)
{
	if (sockaddr->sa_family == AF_INET)
		return &(((struct sockaddr_in *)sockaddr)->sin_addr);
	
	return &(((struct sockaddr_in6 *)sockaddr)->sin6_addr);
}

/*
 * This function can be used to spawn a thread,
 * hence the pointer return.
 *
 * See also: stop_listening (1).
 */
void *listen_start(const int sockfd)
{
	struct sockaddr_storage client_addr;
	socklen_t sin_size;
	
	sin_size = sizeof(client_addr);
	
	if (listen(sockfd, SOMAXCONN) == -1) {
		perror("start_listening(1), listen(2)");
		exit(1);
	}
	
	printf("Server now listening.\n");
	
	while (1) {
		int new_fd;
		
		new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
		
		if (new_fd == -1) {
			perror("start_listening(1), accept(3)");
			continue;
		}
		
		if (!fork()) {
			/* This is the child process. */
			
			/* The child doesn't need the listener. */
			close(sockfd);
			
			/*
			 * Check if this is a connection from a local
			 * service or a remote server.
			 */
			if (socket_is_local((struct sockaddr *)&client_addr) == 0) {
				client_read(new_fd);
			} else {
				connection_log(client_addr);
				server_read(new_fd);
			}
			
			close(new_fd);
			exit(0);
		}
		
		/* The parent doesn't need the file descriptor. */
		close(new_fd);
	}
	
	return 0;
}

/*
 * See also: start_listening (1).
 */
void listen_stop(const int sockfd)
{
	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);
}

void parcel_read(int sockfd, const struct data16 *head_data)
{
	struct data64 *parcel_data;
	ssize_t bytes_read;
	uint64_t parcel_size;
	
	parcel_size = parcel_size_get(head_data);
	parcel_data = (struct data64 *)malloc(sizeof(*parcel_data));
	parcel_data->bytes = (unsigned char *)calloc(parcel_size, sizeof(unsigned char));
	
	if ((bytes_read = read(sockfd, parcel_data->bytes, parcel_size)) == -1)
		perror("read_parcel(2), read(3)");
	
	parcel_data->len = bytes_read;
	
	/***********
	 * PARSING
	 ***********/
	parcel_parse(head_data, parcel_data);
}

void server_read(int sockfd)
{
	struct data16 *head_data;
	ssize_t bytes_read;
	
	head_data = (struct data16 *)malloc(sizeof(*head_data));
	head_data->bytes = (unsigned char *)calloc(DP_PROTO_HOST_HEAD_LEN, sizeof(unsigned char));
	
	/* Wait for a fixed-size header. */
	if ((bytes_read = read(sockfd, head_data->bytes, DP_PROTO_HOST_HEAD_LEN)) == -1)
		perror("read_server(1), read(3)");
	
	/* Header recvd; extract parcel size and read. */
	head_data->len = bytes_read;
	parcel_read(sockfd, head_data);
}

void sigchld_handle(int s)
{
	/* waitpid() might overwrite errno, so we save and restore it. */
	int saved_errno = errno;
	
	while (waitpid(-1, NULL, WNOHANG) > 0);
	
	errno = saved_errno;
}

/*
 * This function checks if a socket is coming
 * from localhost.
 */
int socket_is_local(const struct sockaddr *sockaddr)
{
	if (sockaddr->sa_family == AF_INET) {
		if (((struct sockaddr_in *)sockaddr)->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
			return 0;
		
		return 1;
	} else {
		static const unsigned char localhost_bytes[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
		static const unsigned char mapped_ipv4_localhost[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1 };
		
		if (memcmp(((struct sockaddr_in6 *)sockaddr)->sin6_addr.s6_addr, localhost_bytes, 16) == 0)
			return 0;
		
		if (memcmp(((struct sockaddr_in6 *)sockaddr)->sin6_addr.s6_addr, mapped_ipv4_localhost, 16) == 0)
			return 0;
		
		return 1;
	}
}

/*
 * Opens a new TCP socket and binds it to the given port.
 * Returns the socket file descriptor.
 */
int socket_setup(const char *port)
{
	struct addrinfo *p_info;
	struct addrinfo *info;
	struct addrinfo hints;
	int addr_result;
	int flag_enable;
	int sockfd;
	
	flag_enable = 1;
	sockfd = -1;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;	/* Stream socket */
	hints.ai_flags = AI_PASSIVE;		/* For wildcard IP address */
	hints.ai_protocol = 0;          	/* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	
	if ((addr_result = getaddrinfo(NULL, port, &hints, &info)) != 0) {
		fprintf(stderr, "getaddrinfo, %s", gai_strerror(addr_result));
		perror("");
		exit(1);
	}
	
	/* Loop through all the results and bind to the first we can. */
	for (p_info = info; p_info != NULL; p_info = p_info->ai_next) {
		if ((sockfd = socket(p_info->ai_family, p_info->ai_socktype, p_info->ai_protocol)) == -1) {
			perror("setup_socket(1), socket(3)");
			continue;
		}
		
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag_enable, sizeof(int)) == -1) {
			fprintf(stderr, "setup_sock, TCP setsockopt, port %s", port);
			perror("");
		}
		
		if (bind(sockfd, p_info->ai_addr, p_info->ai_addrlen) == -1) {
			fprintf(stderr, "bind, error binding to TCP port %s", port);
			perror("");
			close(sockfd);
			continue;
		}
		
		break;
	}
	
	if (!p_info) {
		printf("setup_socket(1): TCP socket failed to bind");
		exit(1);
	}
	
	/* Reap all dead processes. */
	grim_reaper();
	freeaddrinfo(info);
	
	return sockfd;
}

/*
 * This function is the main entry point.
 */
void sockets_bootstrap(void)
{
	int sockfd;
	
	sockfd = socket_setup(DP_PORT);
	listen_start(sockfd);
}
