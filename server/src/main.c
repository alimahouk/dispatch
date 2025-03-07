//
//  main.c
//  server
//
//  Created by Ali Mahouk on 12/8/17.
//  Copyright © 2017 Ali Mahouk. All rights reserved.
//

#include "disk.h"
#include "net.h"
#include "protocol.h"
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>


/********************
 * Global Variables
 ********************/
/*
 * I would really like to avoid global variables
 * as much as possible. Keep the whole system as
 * stateless as possible.
 */
struct path *path_dir_root;
/**********************/

/**********************
 * Private Prototypes
 **********************/
void *schedule(void *);
void time_out(void);
/**********************/


int main(int argc, const char *argv[])
{
	pthread_t t_sched;
	
	path_dir_root = directories_bootstrap();
	pthread_create(&t_sched, 0, schedule, 0);
	
	sockets_bootstrap();
	
	return 0;
}

void *schedule(void *args)
{
	/*
	 * SCHEDULING
	 * --
	 * Upon SIGALRM, call time_out().
	 * Set interval timer. We want frequency in ms,
	 * but the setitimer call needs seconds and useconds.
	 * For every activity that needs to be called after an
	 * interval, spawn a thread for it in time_out().
	 */
	struct itimerval it_val;
	
	/*
	 * Need to use a global variable because fucking signal(2)
	 * does not allow for passing arguments.
	 */
	if ( signal(SIGALRM, (void (*)(int))time_out) == SIG_ERR )
		printf("Unable to catch SIGALRM");
	
	it_val.it_value.tv_sec  = DP_DIR_SCAN_INT / 1000;
	it_val.it_value.tv_usec = (DP_DIR_SCAN_INT * 1000) % 1000000;
	it_val.it_interval      = it_val.it_value;
	
	if ( setitimer(ITIMER_REAL, &it_val, NULL) == -1 )
		printf("Error calling setitimer()");
	
	/* Initiate the first call right away. */
	time_out();
	
	return 0;
}

void time_out(void)
{
	pthread_t t_chkdir;
	
	pthread_create(&t_chkdir, 0, directory_tree_scan, (void *)path_dir_root);
}
