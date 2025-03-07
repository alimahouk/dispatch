//
//  net.h
//  server
//
//  Created by Ali Mahouk on 12/8/17.
//  Copyright © 2017 Ali Mahouk. All rights reserved.
//

#ifndef NET_H
#define NET_H


#include "types.h"


/*************
 * CONSTANTS *
 *************/
static const char *DP_PORT = "1992";

/*************
 * FUNCTIONS *
 *************/
int data64_send(const char *, const struct data16 *, const struct data64 *);
void *listen_start(const int);
void listen_stop(const int);
void sockets_bootstrap(void);


#endif /* NET_H */
