
# TraceIT

A tool for tracing and debugging already running programs.

It works by using defines for logging information in your C-code in the
format

> TRACE(LOG_DEBUG, "This is a test");

If the above is run, by default you will get the output send to your
syslog, see section "Environment Variables" for more info.

Available log levels are:

 * LOG_EMERG
 * LOG_ALERT
 * LOG_CRIT
 * LOG_ERR
 * LOG_WARNING
 * LOG_NOTICE
 * LOG_INFO
 * LOG_DEBUG
	
If no `trace.conf` file is loaded only log level above LOG_INFO will be
output.

### trace.conf

Configuration file common for all programs using TraceIT, unless you
choose to specify otherwise using environment variables. Default
location is `/etc/trace.conf`

Format of traceconf is basically:

	<programname> <sourcefile> <function>
	
Example:
	
	wolf foobar.c hello

would enable tracing of program `wolf` in the file `foobar.c` for the
function `hello`.

Wildcards are allowed so a good starting for debugging your program:
	
	wolf * *

Would enable trace for all files in program `wolf`, and for all
functions.

To toggle debugging on and off prepend the line with `+/-`, so to
disable trace for `wolf` do:

	- wolf * *

Also, wildcards can be used in function and file names, so valid
trace.conf would be:

	wolf foo*.[Cc] fun_*


### Environment variables

`TRACEFILE` can be set as an environment variable. If you would like to
trace to a specific file:

	TRACEFILE=/var/log/myfile

or if you would like output to stdout, simple:

	TRACEFILE=

`TRACECONF` can be used as an alternative path to the TraceIT
configuration file:

	TRACECONF=/etc/alttrace.conf

## Usage, the important files

### libtrace.a

Library to link against. 

### traceit

A shell script provided that is used to change the content of trace.conf on the fly.

## License

Distributed under LGPL. Use it, but don't change the license and please
send constructive feedback and changes/additions back.

For the greater good..

## Contact

Jesper L. Nielsen <lyager\gmail.com>

