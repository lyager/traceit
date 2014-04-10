/* 
 * trace.c -- A simple trace facility for multi-threaded programs.
 *
 * vim:sw=3
 *
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <fnmatch.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>

#include "trace.h"

/*
 * Constants:
 */

/* The initial state for the trace point. */
#define STATE_INIT   -1
/* The state after the user have requested a reload. */
#define STATE_REINIT -2
/* State one is the first state, the state is incremented each time we
   trace.  */
#define STATE_ONE     1
/* Trace is off, the trace_at_ function is not called. */
#define STATE_OFF     0

/*
 * Types:
 */

/* This structure represent an entry in the trace configuration file. */
struct trace_conf {
    bool enable;		/* Enable or disable if it match. */
    char *file_pattern;
    char *func_pattern;
    struct trace_conf *next;
};

/*
 * Local variables:
 */

/* A pointer to the first trace point structure in the single linked list.
   This is always the last initialized trace point. */
static struct trace_point_ *trace_point_head = NULL;

/* A pointer to the first and last trace configuration structure.  */
static struct trace_conf *trace_conf_head = NULL;
static struct trace_conf *trace_conf_tail = NULL;

/* Use the syslog facility for trace messages. */
static bool use_syslog = true;

/* Output file pointer and name if syslog isn't used. */
static char *trace_file = NULL;
static FILE *trace_fp = NULL;

/* System log level for internal messages. */
static int system_level = LOG_INFO;

/* A big fat mutex used around the trace function. */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Local prototypes:
 */

static void message(int priority, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

static void system_trace(int priority, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

/*
 * Local functions:
 */

/* Write a message to syslog or to the output file. */
static void message(int priority, const char *format, ...)
{
    va_list args;

    va_start(args, format);

    if (use_syslog)
	vsyslog(priority, format, args);
    else {
	FILE *fp = trace_fp ? trace_fp : stderr;
	vfprintf(fp, format, args);
	putc('\n', fp);
	fflush(fp);
    }
    va_end(args);
}

/* Convert the syslog priority integer value as a string. */
static char *priority_to_str(int priority)
{
    switch (priority) {
    case LOG_EMERG:
	return "Emerg";
    case LOG_ALERT:
	return "Alert";
    case LOG_CRIT:
	return "Critical";
    case LOG_ERR:
	return "Error";
    case LOG_WARNING:
	return "Warning";
    case LOG_NOTICE:
	return "Notice";
    case LOG_INFO:
	return "Info";
    case LOG_DEBUG:
	return "Debug";
    default:
	return "?";
    }
    return NULL;
}

/* Internal message from the trace system itself. */
static void system_trace(int priority, const char *format, ...)
{
    va_list args;
    char *msg;

    /* Ignore low priority system message. */
    if (priority > system_level)
	return;

    va_start(args, format);
    vasprintf(&msg, format, args);
    va_end(args);

    if (use_syslog)
	syslog(priority, "Trace : %s[%d]: %s",
	       priority_to_str(priority), getpid(), msg);
    else {
	FILE *fp = trace_fp ? trace_fp : stderr;
	fprintf(fp, "[%s] Trace %s[%d]: %s\n",
		program_invocation_short_name,
		priority_to_str(priority), getpid(), msg);
	fflush(fp);
    }
    free(msg);
}

/* Yea or nay. */
static bool str_to_bool(const char *value)
{
    return !(strcasecmp(value, "false") == 0
	     || strcasecmp(value, "no") == 0
	     || strcasecmp(value, "off") == 0
	     || strcasecmp(value, "nil") == 0
	     || strcasecmp(value, "nay") == 0
	     || strcasecmp(value, "-") == 0
	     || strncmp(value, "0", 1) == 0);
}

/* Prepend an entry to the configuration list. */
static void add_conf(bool enable,
		     const char *file_pattern,
		     const char *func_pattern)
{
    struct trace_conf *conf;

    system_trace(LOG_DEBUG,
		 "Conf %i \"%s\", \"%s\"",
		 enable, file_pattern, func_pattern);

    conf = malloc(sizeof(struct trace_conf));
    assert(conf);
    conf->enable = enable;
    conf->file_pattern = strdup(file_pattern);
    assert(conf->file_pattern);
    if (func_pattern)
	conf->func_pattern = strdup(func_pattern);
    else
	conf->func_pattern = NULL;
    conf->next = NULL;
    if (trace_conf_tail)
	trace_conf_tail->next = conf;
    else
	trace_conf_head = conf;
    trace_conf_tail = conf;
}

/* Set a configuration value. */
static void set_conf(const char *name, const char *value)
{
    if (strcmp(name, "file") == 0) {
	if (trace_file && trace_fp)
	    fclose(trace_fp);
	free(trace_file);
	trace_file = strdup(value);
	if (trace_file) {
	    trace_fp = fopen(trace_file, "a");
	    if (trace_fp == NULL)
		system_trace(LOG_ERR,
			     "Cannot open trace output file: %s",
			     trace_file);
	}
    }
    else if (strcmp(name, "syslog") == 0)
	use_syslog = str_to_bool(value);
}

/* Check the configuration list to see if trace is on for the given
   trace point.  */
static bool is_trace_on(const char *file, const char *func,
			int line __attribute__((unused)))
{
    struct trace_conf *conf;
    bool enable = false;

    for (conf = trace_conf_head; conf; conf = conf->next)
	if (fnmatch(conf->file_pattern, file, 0) == 0
	    && fnmatch(conf->func_pattern, func, 0) == 0)
	    enable = conf->enable;
    return enable;
}

/* Load or reload the trace configuration file. */
static void load_conf(void)
{
    char *conf_file = NULL;
    int conf_line_no;
    FILE *conf_fp;
    char *text = NULL;
    size_t len = 0;

    /* Get the configuration file name. */
    conf_file = getenv("TRACECONF");
    if (conf_file == NULL)
	conf_file = "/etc/trace.conf";

    system_trace(LOG_INFO, "Reading trace configuration: %s", conf_file);

    /* Open trace configuration file. */
    conf_fp = fopen(conf_file, "r");
    if (conf_fp == NULL) {
	if (errno != ENOENT)
	    system_trace(LOG_WARNING,
			 "Cannot open trace file: %s: %m", conf_file);
	return;
    }

    /* Clear the old trace configuration. */
    if (trace_conf_head) {
	struct trace_conf *conf;

	conf = trace_conf_head;
	trace_conf_head = NULL;
	trace_conf_tail = NULL;
	while (conf) {
	    struct trace_conf *next;
	    next = conf->next;
	    free(conf->file_pattern);
	    free(conf->func_pattern);
	    free(conf);
	    conf = next;
	}
    }

    /* Read new configuration file. */
    for (conf_line_no = 1; true; conf_line_no++) {
	char *s;
	ssize_t n;
	char *token_vector[10] = { NULL };
	unsigned int token_count;

	n = getline(&text, &len, conf_fp);

	if (n == -1)
	    break;

	if (n == 0)		/* An empty line. */
	    continue;

	/* Remove the newline.  But be careful the last line might be
	   without a newline. */
	if (text[n-1] == '\n')
	    text[n-1] = '\0';

	/* Strip comment. */
	s = strchr(text, '#');
	if (s)
	    *s = '\0';

	/* Split into tokens. */
	s = text;
	for (token_count = 0;
	     token_count < (sizeof(token_vector)/sizeof(token_vector[0]) - 1);
	     token_count++) {
	    do
		token_vector[token_count] = strsep(&s, " \t,:");
	    while (token_vector[token_count]
		   && *(token_vector[token_count]) == '\0');
	    if (token_vector[token_count] == NULL)
		break;
	}

	if (token_count == 0)
	    continue;		/* Ignore empty or comment lines. */

	{
	    bool on = true;
	    int offset = 0;

	    /* Get the optional + or - prefix. */
	    if (strncmp(token_vector[0], "-", 1) == 0) {
		on = false;
		offset = 1;
	    } else if (strncmp(token_vector[0], "+", 1) == 0) {
		offset = 1;
	    }

	    if (fnmatch(token_vector[offset],
			program_invocation_short_name, 0))
		continue;	 /* Ignore settings for other programs. */

	    /* <program> <name> = <value> */
	    if (offset == 0
		&& token_count == 4
		&& strncmp(token_vector[2], "=", 1) == 0)
		set_conf(token_vector[1], token_vector[3]);
	    else {
		switch (token_count - offset) {
		case 2: /* [+|-] <program> <function> */
		    add_conf(on, "*", token_vector[1 + offset]);
		    break;
		case 3: /* [+|-] <program> <file> <function> */
		    add_conf(on,
			     token_vector[1 + offset],
			     token_vector[2 + offset]);
		    break;
		default:
		    system_trace(LOG_WARNING,
				 "%s:%d: Bad trace configuration line",
				 conf_file, conf_line_no);
		}
	    }
	}
    }
    free(text);
    fclose(conf_fp);
}

/* Update the trace point state using the values in the configuration. */
static void update_point(struct trace_point_ *point,
			 const char *file,
			 const char *func,
			 int line,
			 int priority)
{
    if (point->state < 0) {		/* STATE_INIT or STATE_REINIT */
	if (point->state == STATE_INIT) {
	    /* A new tracepoint, so we prepend it to the trace point
	       list. */
	    point->next = trace_point_head;
	    trace_point_head = point;
	}
	if (priority <= LOG_NOTICE || is_trace_on(file, func, line))
	    point->state = STATE_ONE; /* Trace is on for this trace point. */
	else
	    point->state = STATE_OFF; /* No trace for this trace point. */
    }
}

/* Dummy signal handler.  */
static void handler(int num __attribute__((unused)))
{
    // - Enable to debug SIGUSR receival.
    //system_trace(LOG_INFO, "Got SIGURG");
}

/*
 * Global function:
 */

/* Initialize trace library. */
void trace_init(void)
{
    static bool inited = false;
    struct sigaction sa;

    pthread_mutex_lock(&lock);
    if (!inited) {
	inited = true;
	char *file = getenv("TRACEFILE");
	if (file && strlen(file) == 0)
	    use_syslog = false;
	load_conf();
	/* Register an empty signal handler for SIGURG, this is needed
	   so that sigwait function can receive the signal. */
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGURG, &sa, NULL);
    }
    pthread_mutex_unlock(&lock);
}

/* Reload trace configuration. */
void trace_load(void)
{
    struct trace_point_ *point;
    pthread_mutex_lock(&lock);
    for (point = trace_point_head; point; point = point->next)
	point->state = STATE_REINIT;
    load_conf();
    pthread_mutex_unlock(&lock);
}

/* This function is used to implement the TRACE_IS_ON, TRACE_IS_OFF,
   and TRACE_FOR macros. */
void trace_state_at_(struct trace_point_ *point,
		     const char *file, const char *func, int line)
{
    pthread_mutex_lock(&lock);
    /* Just update the trace point state so that the macro can read
       the trace point state.  */
    update_point(point, basename(strdupa(file)), func, line, LOG_DEBUG);
    pthread_mutex_unlock(&lock);
}

/* This is the trace implementation, it should only be called from the
   TRACE macro.  */
void trace_at_(struct trace_point_ *point,
	       const char *file, const char *func, int line,
	       int priority,
	       const char *format, ...)
{
    va_list args;

    assert(point);
    assert(file);
    assert(func);

    pthread_mutex_lock(&lock);

    file = basename(strdupa(file));

    /* Update point if necessary. */
    update_point(point, file, func, line, priority);

    /* If state is true then write trace message. */
    if (point->state) {
	static char prefix_buf[200];
	static char *message_buf = NULL;
	static size_t message_size = 0;

	/* First format a prefix string for the message. */
	if (use_syslog)
	    snprintf(prefix_buf, sizeof(prefix_buf),
		     "%s:%d:%s %s[%d/%d",
		     file, line, func,
		     priority_to_str(priority),
		     getpid(),
		     point->state);
	else
	    snprintf(prefix_buf, sizeof(prefix_buf),
		     "[%s] %s:%d:%s %s[%d/%d",
		     program_invocation_short_name,
		     file, line, func,
		     priority_to_str(priority),
		     getpid(),
		     point->state);

	if (format) {
	    int n;		/* The size of the message string. */
	    char *s, *q;

	    va_start(args, format);

	    /* Initialize the static message buffer.*/
	    if (message_buf == NULL) {
		message_size = 100;
		message_buf = malloc(message_size);
		assert(message_buf);
	    }

	    /* Format the message text, increase message buffer size
	       if necessary. */
	    n = vsnprintf(message_buf, message_size, format, args);
	    if (n >= (int)message_size) {
		message_size = 2*n;
		free(message_buf);
		message_buf = malloc(message_size);
		assert(message_buf);
		n = vsnprintf(message_buf, message_size, format, args);
	    }

	    va_end(args);

	    /* If the message contains newlines then print a message
	       for each line. */
	    s = message_buf;
	    q = strchr(s, '\n');
	    if (n && q) {
		int i = 1;	/* Line number. */
		for (;;)
		{
		    if (q != NULL) {
			if (*(q - 1) == '\r') {
			    *(q - 1) = '\0';
			}
			*q = '\0';
		    }
		    message(priority, "%s/%d]: %s", prefix_buf, i++, s);
		    if (q == NULL)
			break;
		    s = q + 1;
		    q = strchr(s, '\n');
		    if (*s == '\0')
			break;
		}
            }
	    else
		message(priority, "%s]: %s", prefix_buf, message_buf);
	} else			/* A NULL format. */
	    message(priority, "%s]", prefix_buf);

	/* Handle state count overflow. */
	if (++(point->state) <= 0)
	    point->state = STATE_ONE;
    }
    pthread_mutex_unlock(&lock);
}
