#include "trace.h"

int main(void)
{

	trace_init();
	TRACE(LOG_DEBUG, "Hi");
	TRACE(LOG_DEBUG, "This is main.");
	return 0;
}

