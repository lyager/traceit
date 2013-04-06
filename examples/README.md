
# nothread.bin

Have a look at nothread.c, which is probably the most simple way of using TraceIT.

Running ./nothread.bin gives no output. That's the whole point. Try invoking it like

	TRACEFILE= TRACECONF=nothread.conf ./nothread.bin 

This should give you some output to stdout. When TRACEFILE is set to the empty
string TraceIT logs to stdout. Otherwise it would log to syslog, or to a
filename: TRACEFILE=<filename>

TRACECONF tells which configuration file to use. Default is /etc/trace.conf. For more
info on the configuration file format have a look at src/trace.h. The example
traceconf in this example tells TraceIT to log the binary 'nothread.bin' and
the function 'main'.

# thread_loop.bin

A little more realistic example. A thread called 'hi' runs always, while the
main loop looks after the SIGURG signal coming in. Upon receiving the SIGURG
signal the main loop call trace_load() and thus reloads the configuration
file.

Run it like

	TRACEFILE= TRACECONF=thread_loop.conf ./thread_loop.bin 

You should be able to see the debug info coming from the 'hi' thread, Now try
modifying 'thread_loop.conf' changing

	thread_loop.bin hi

to

	- thread_loop.bin hi

and now send the SIGURG signal to the program

	kill -URG `pidof thread_loop.bin`

The debug info from the 'hi' thread should now disappear.
