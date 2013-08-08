#ifndef TRACE_H
#define TRACE_H

/*
 * This is a simple trace facility for multi-threaded programs.
 *
 * Created by Jesper L. Nielsen <lyager\gmail.com>
 *
 * License: LGPL
 *
 * API
 * ---
 *
 * Include this 'trace.h' header.
 * Initialize the trace library using the `trace_init()' function.
 * The SIGURG signal is used to signal a configuration file update.
 * Use the `trace_load()' function to reload the configuration file.
 * Note that it is not allowed to call `trace_load()' from a signal
 * handler. Instead the `sigwait()' function must be used to wait for
 * the reload signal.  The SIGURG signal should be blocked in all
 * threads for this to work.
 *
 * Write trace messages using the `TRACE(...)' macro.
 *
 * The `TRACE_IS_OFF()' macro returns true if trace is off for the
 * current function.
 *
 * The `TRACE_IS_ON()' macro returns true if trace is on for the
 * current function.
 *
 * The `TRACE_FOR(minor, major)' returns true if trace is on for
 * the two major and minor strings as specified in the trace
 * configuration file.
 *
 * Configuration
 * -------------
 *
 * The default configuration file is /etc/trace.conf
 * The format of the trace file is
 *   [+|-] <program> <source file> <function>
 * The entries are separated by whitespace or ':'.
 * For example the following line enables trace in the function
 * `hello' in the file 'foobar.c' for the application `wolf'.
 *   + wolf foobar.c:hello
 *
 * To disable trace for a trace point that already was enabled by a
 * previous configuration line, use a - entry. For example
 *   + wolf foobar.c *
 *   - wolf foobar.c hello
 * enables all trace points in foobar.c except the function hello.
 *
 * Wildcards can be used to match program, source, and function name.
 * For example
 *   + hello bar*.[Cc] fun_*
 *
 * The enable/disable mark ('+'/'-') is optional, the default is to 
 * enable trace.
 *
 * If only two arguments are specified then <source file> is
 * set to "*". Indeed 
 *   <program> <function>
 * is the same as
 *   <program> * <function>
 *
 * For the TRACE_FOR(minor, major) macro the configuration format is
 *   [+|-] <program> <minor> <major>
 *
 * User Control
 * ------------
 *
 * To force a reload the trace configuration file, send the SIGURG
 * signal to the process.  This signal is ignored per default, so no
 * harm is done even when the process is not using the trace facility
 * or if the trace facility has not yet been initialized.
 *
 *   kill -URG `pidof example`
 *
 * By default log messages are sent to syslog. This can be changed using
 * the environment variable TRACEFILE. 
 *
 *   Log to stderr
 *     TRACEFILE= 
 *
 *   Log to file
 *     TRACEFILE=<FILENAME>
 *
 * The default location for the configuration file is /etc/trace.conf,
 * this can be changed with the environment variable TRACECONF.
 *
 * The tracit Program
 * ------------------
 *
 * The tracit program can be used to add or remove entries from the
 * /etc/trace.conf file. 
 *
 */

#include <signal.h>
#include <syslog.h>

/* This is the main trace macro. */
#define TRACE(priority, format, args...)			\
  ({static struct trace_point_ trace_point_ = { -1, NULL };	\
    if (trace_point_.state)					\
      trace_at_(&trace_point_,					\
		__FILE__, __FUNCTION__, __LINE__,		\
		priority,					\
		format, ## args);})

#define TRACE_IS_ON()						\
  ({static struct trace_point_ trace_point_ = { -1, NULL };	\
    if (trace_point_.state < 0)					\
      trace_state_at_(&trace_point_,				\
		      __FILE__, __FUNCTION__, __LINE__);	\
    trace_point_.state;})

#define TRACE_IS_OFF()						\
  ({static struct trace_point_ trace_point_ = { -1, NULL };	\
    if (trace_point_.state < 0)					\
      trace_state_at_(&trace_point_,				\
		      __FILE__, __FUNCTION__, __LINE__);	\
    !trace_point_.state;})

#define TRACE_FOR(minor, major)					\
  ({static struct trace_point_ trace_point_ = { -1, NULL };	\
    if (trace_point_.state < 0)					\
      trace_state_at_(&trace_point_,				\
		      minor, major, -1);			\
    trace_point_.state;})

/* Convenient macros to print variables: */
#define TRACE_VAR_INT(priority, var)			\
  TRACE(priority, "Variable: " #var ": %d", (var))
#define TRACE_VAR_PTR(priority, var)			\
  TRACE(priority, "Variable: " #var ": %p", (var))
#define TRACE_VAR_CHR(priority, var)			\
  TRACE(priority, "Variable: " #var ": \'%c\'", (var))
#define TRACE_VAR_BOOL(priority, var)			\
  TRACE(priority, "Variable: " #var ": %s", (var) ? "true" : "false")
#define TRACE_VAR_STR(priority, var)			\
  TRACE(priority, "Variable: " #var ": %s%s%s (%p)",	\
	var ? "\"" : "",				\
	var ? var : "NULL",				\
	var ? "\"" : "",				\
	var)

/* Internal trace point structure, do not mess with this structure. */
struct trace_point_
{
  int state;
  struct trace_point_ *next;
};

/* Initialize the trace library. */
extern void trace_init(void);

/* Load new configuration.  */
extern void trace_load(void);

/* Internal trace function, do not use this function directly
   use the trace `macro' instead. */
extern void trace_at_(struct trace_point_ *point,
		      const char *file, const char *func, int line,
		      int priority,
		      const char *format, ...)
  __attribute__((format(printf, 6, 7)));

/* Internal function for initializing the trace point state. */
extern void trace_state_at_(struct trace_point_ *point,
			    const char *file, const char *func, int line);

#endif /* trace.h */
