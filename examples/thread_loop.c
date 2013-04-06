#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include "trace.h"

void *hi(void *arg __attribute__((unused)))
{
	for(;;) {
		fprintf(stderr, "Normal print from '%s'\n", __FUNCTION__);
		TRACE(LOG_DEBUG, "%s", __FUNCTION__);
		sleep(1);
	}
	return NULL;
}

int main(void)
{
	pthread_t hithread;
	sigset_t set;

	trace_init();
	sigemptyset(&set);
	sigaddset(&set, SIGURG);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	pthread_create(&hithread, NULL, hi, NULL);

	for ( ; ; ) {
		int signo;

		sigwait(&set, &signo);
		switch(signo) {
		case SIGURG:
			trace_load();
			break;
		default:
			return EXIT_SUCCESS;
		}	
		TRACE(LOG_DEBUG, "Hi");
		TRACE(LOG_DEBUG, "This is main.");
		sleep(1);
	}
	return EXIT_SUCCESS;
}

