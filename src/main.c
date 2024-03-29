#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

#include <regex.h>

#include <libircclient/libircclient.h>

#include <jansson.h>

#include "conf.h"


// -----------------------------------------------------------------------------------------------
// global
sig_atomic_t g_askedToStop = 0;

// -----------------------------------------------------------------------------------------------

typedef enum {
	STATE_UNCREATED = 1,
	STATE_CREATED = 2,
	STATE_CONNECTED = 3,
	STATE_DESCRIPTOR_ADDED = 4,
	STATE_CONNECTION_ERROR = 5,
	STATE_WAIT_TO_RECONNECT = 6,
	STATE_STOPPING = 7
} irc_session_state_t;

typedef struct {
	irc_session_state_t state;
	irc_session_t * s;

	server_conf_t * server_conf;

	struct timeval wait_date;

} irc_ctx_t;

typedef struct {
	irc_conf_t * irc_conf;
	irc_callbacks_t	callbacks;
	irc_ctx_t * servers_ctx;
	int servers_count;
	struct timeval tv;
	fd_set in_set, out_set;
	int maxfd;
} irc_common_ctx_t;

void addlog(const char * fmt, ...) {
	char buf[1024];
	va_list va_alist;

	va_start(va_alist, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va_alist);
	va_end(va_alist);

	printf("%s\n", buf);
}

int regex_conf_compile(regex_conf_t* regex_conf, regex_t * regex) {
    const char * regex_str = regex_conf_get_regex(regex_conf);
    int result = regcomp(regex, regex_str, REG_EXTENDED | REG_ICASE);
    if (result)
    {
        char buf[512];
        regerror(result, regex, buf, sizeof(buf));
        fprintf(stderr, "Invalid regular expression: %s", buf);
        regfree(regex);
        return 0;
    } else {
        printf("Compilation ok : %s => %p\n", regex_str, regex);
    }
    return 1;
}

int match_filter(const char** lines, int lines_count, filter_conf_t * filter_conf) {
    int regexes_count = filter_conf_get_regexes_count(filter_conf);
    if (lines_count < regexes_count) {
        printf("No match, not enough lines.\n");
        return 0;
    }

    int i;
    for (i = 0; i < lines_count && i < regexes_count; i++) {
        regex_conf_t * regex_conf = filter_conf_get_regex_at(filter_conf, i);
        regex_t regex;
        if (!regex_conf_compile(regex_conf, &regex)) {
            return 0;
        }

        if (regex_conf_get_vars_count(regex_conf) != regex.re_nsub) {
            printf("Wrong number of variable, re: %d, vars: %d\n", regex.re_nsub,
                regex_conf_get_vars_count(regex_conf));
            regfree(&regex);
            return 0;
        }


        size_t ngroups = regex.re_nsub + 1;
        regmatch_t *groups = malloc(ngroups * sizeof(regmatch_t));

        printf("matching line %d : %s with %s, ngroups = %d, regex = %p\n",
            i, lines[i], regex_conf_get_regex(regex_conf), ngroups, &regex);

        int result = regexec(&regex, lines[i], ngroups, groups, 0);
        if (!result)
        {
            printf("line %d match -> %s\n", i, lines[i]);
            int j;
            for (j = 1; j <= regex.re_nsub; j++)
            {
                if (groups[j].rm_so != -1)
                {
                    printf("subgroup %2d from %2d to %2d: \"%.*s\", var = %s\n", j, groups[j].rm_so,
                            groups[j].rm_eo, groups[j].rm_eo - groups[j].rm_so, lines[i]
                            + groups[j].rm_so, regex_conf_get_var_at(regex_conf, j - 1));
                }

            }
        } else {
            printf("No match, line %d : %s != %s\n", i, lines[i], regex_conf_get_regex(regex_conf));
            free(groups);
            regfree(&regex);
            return 0;
        }
        free(groups);
        regfree(&regex);
    }
    return 1;
}

void dump_event (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	char buf[512];
	int cnt;

	buf[0] = '\0';

	for ( cnt = 0; cnt < count; cnt++ ) {
		if ( cnt ) {
			strcat (buf, "|");
		}
		strcat (buf, params[cnt]);
	}

	addlog ("Event \"%s\", origin: \"%s\", params: %d [%s]", event, origin ? origin : "NULL", cnt, buf);
}

void event_connect (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	dump_event(session, event, origin, params, count);

	irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);

	// Send commands
	int cmd_idx;
	for (cmd_idx = 0; cmd_idx < server_conf_get_cmds_count(ctx->server_conf); cmd_idx++) {
		cmd_conf_t * cmd_conf = server_conf_get_cmd_at(ctx->server_conf, cmd_idx);
		const char* cmd_name = cmd_conf_get_name(cmd_conf);
		const char* cmd_arg1 = cmd_conf_get_arg1(cmd_conf);
		const char* cmd_arg2 = cmd_conf_get_arg2(cmd_conf);

		if (!strcmp(cmd_name, "msg")) {
			printf("Sending msg to %s\n", cmd_arg1);
			irc_cmd_msg(session, cmd_arg1, cmd_arg2);
		} else {
			fprintf(stderr, "WARN: unsupported command %s\n", cmd_name);
		}
	}

	// Join channels
	int chan_idx;
	for (chan_idx = 0; chan_idx < server_conf_get_channels_count(ctx->server_conf); chan_idx++) {
		channel_conf_t * channel_conf = server_conf_get_channel_at(ctx->server_conf, chan_idx);

		const char* chan_name = channel_conf_get_name(channel_conf);
		const char* chan_pass = channel_conf_get_passwd(channel_conf);
		printf("Joining %s\n", chan_name);

		irc_cmd_join (session, chan_name, chan_pass);
	}
}


void event_channel (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	if (count != 2) {
		return;
	}

	if (!origin) {
		fprintf(stderr, "WARN: No origin for following event:");
		dump_event(session, event, origin, params, count);
		return;
	}

	irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);

	int chan_idx;
	for (chan_idx = 0; chan_idx < server_conf_get_channels_count(ctx->server_conf); chan_idx++) {
		channel_conf_t * channel_conf = server_conf_get_channel_at(ctx->server_conf, chan_idx);

		const char* chan_name = channel_conf_get_name(channel_conf);
		if (!strcmp(chan_name, params[0])) {
			const char* nickfilter = channel_conf_get_nickfilter(channel_conf);
			if (nickfilter) {
				if(!strcmp(nickfilter, origin)) {
					// match filters on channel history

					int filters_count = channel_conf_get_filters_count(channel_conf);
					int filter_idx;
					printf("filters_count = %d\n", filters_count);
				    for (filter_idx = 0; filter_idx < filters_count; filter_idx++) {
				        filter_conf_t * filter_conf = channel_conf_get_filter_at(channel_conf, filter_idx);
				        match_filter(&params[1], 1, filter_conf);
				    }
				}
			} else {
				// match filters on channel history
			}
		}
	}

	printf ("%s:%s: %s\n", origin ? origin : "someone", params[0], params[1] );
}

void event_kick (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	dump_event(session, event, origin, params, count);
//	irc_ctx_t * ctx = (irc_ctx_t *) irc_get_ctx (session);
//	irc_cmd_join (session, ctx->channel, 0);
}

void event_numeric (irc_session_t * session, unsigned int event, const char * origin, const char ** params, unsigned int count)
{
	if ( event > 400 )
	{
		printf ("ERROR %d: %s: %s %s %s %s\n",
				event,
				origin ? origin : "unknown",
				params[0],
				count > 1 ? params[1] : "",
				count > 2 ? params[2] : "",
				count > 3 ? params[3] : "");
	}
}

void initCallbacks(irc_callbacks_t* callbacks) {
	memset (callbacks, 0, sizeof(irc_callbacks_t));

	callbacks->event_connect = event_connect;
	callbacks->event_channel = event_channel;
	callbacks->event_kick = event_kick;

	//callbacks->event_join = dump_event;
	//callbacks->event_quit = dump_event;
	//callbacks->event_part = dump_event;

	//callbacks->event_nick = dump_event;
	//callbacks->event_mode = dump_event;
	//callbacks->event_topic = dump_event;
	//callbacks->event_privmsg = dump_event;
	callbacks->event_notice = dump_event;
	callbacks->event_invite = dump_event;
	//callbacks->event_umode = dump_event;
	//callbacks->event_ctcp_req = dump_event;
	//callbacks->event_ctcp_rep = dump_event;
	//callbacks->event_ctcp_action = dump_event;
	callbacks->event_unknown = dump_event;
	callbacks->event_numeric = event_numeric;
}

typedef int (*sessionActionFunc_t)(irc_common_ctx_t* , irc_ctx_t* );

int doAction(irc_common_ctx_t* common_ctx, sessionActionFunc_t sessionActionFunc) {
	int result = 0;
	int i;
	for (i = 0; i < common_ctx->servers_count; i++) {
		int actionResult = sessionActionFunc(common_ctx, &common_ctx->servers_ctx[i]);
		// We want to process all ctxs even if there is an error.
		result &= actionResult;
	}
	return result;
}

int doCreation(irc_common_ctx_t* common_ctx, irc_ctx_t* ctx) {
	if (ctx->state == STATE_UNCREATED) {
		printf("Creating session for %s\n", server_conf_get_name(ctx->server_conf));
		ctx->s = irc_create_session (&common_ctx->callbacks);
		if (!ctx->s) {
			printf("FATAL: Could not create IRC session.\n");
			return 0;
		}
		irc_set_ctx (ctx->s, ctx);
		irc_option_set(ctx->s, LIBIRC_OPTION_STRIPNICKS);
		ctx->state = STATE_CREATED;
	}
	return 1;
}

int doConnection(irc_common_ctx_t* common_ctx, irc_ctx_t* ctx) {
	if (ctx->state == STATE_CREATED) {
		const char* server_ip = server_conf_get_ip(ctx->server_conf);
		const int server_port = server_conf_get_port(ctx->server_conf);
		const char* server_pass = server_conf_get_passwd(ctx->server_conf);
		const char* server_nick = server_conf_get_nick(ctx->server_conf);

		printf("Connecting to %s:%d\n", server_ip, server_port);
		if (irc_connect(ctx->s, server_ip, server_port, server_pass, server_nick, 0, 0)) {
			printf("FATAL: Could not connect: %s\n", irc_strerror(irc_errno(ctx->s)));
			return 0;
		}
		ctx->state = STATE_CONNECTED;
	}
	return 1;
}

int doAddDescriptors(irc_common_ctx_t* common_ctx, irc_ctx_t* ctx) {
	if (ctx->state == STATE_CONNECTED) {
		if (irc_add_select_descriptors (ctx->s, &common_ctx->in_set, &common_ctx->out_set, &common_ctx->maxfd)) {
			printf("ERROR: irc_add_select_descriptors() : %s (%d)\n",
					irc_strerror(irc_errno(ctx->s)), irc_errno(ctx->s));
			return 0;
		}
		ctx->state = STATE_DESCRIPTOR_ADDED;
	}
	return 1;
}

int doSelect(irc_common_ctx_t* common_ctx) {
	do {
		int retval = select (common_ctx->maxfd + 1, &common_ctx->in_set, &common_ctx->out_set, 0, &common_ctx->tv);
		if ( retval < 0 ) {
			// Does something really bad happened?
			if ( errno == EINTR ) {
				// No, select was just interrupted by a signal.
				fprintf(stderr, "I");
				if (g_askedToStop) {
					int i;
					for (i = 0; i < common_ctx->servers_count; i++) {
						common_ctx->servers_ctx[i].state = STATE_STOPPING;
					}
					return 0;
				}
				continue;
			}
			// Yes, get out of here.
			printf("FATAL select error : %s.\n", strerror(errno));
			return 0;
		} else if (retval == 0) {
			// Timeout
			//fprintf(stderr, "T");
		}

		// Everything OK.

	} while(0); // to be able to use "continue" when interrupted

	return 1;
}

int doProcessDescriptors(irc_common_ctx_t* common_ctx, irc_ctx_t* ctx) {
	if (ctx->state == STATE_DESCRIPTOR_ADDED) {

		if ( irc_process_select_descriptors (ctx->s, &common_ctx->in_set, &common_ctx->out_set)) {
			printf("ERROR: irc_process_select_descriptors() : %s (%d)\n",
					irc_strerror(irc_errno(ctx->s)), irc_errno(ctx->s));
			ctx->state = STATE_CONNECTION_ERROR;

			if (gettimeofday(&ctx->wait_date, NULL)) {
				printf("ERROR: gettimeofday() : %s.\n", strerror(errno));
			} else {
				ctx->wait_date.tv_sec += 2;
				ctx->wait_date.tv_usec += 0;
			}

			return 0;
		}

		ctx->state = STATE_CONNECTED;

	}
	return 1;
}

int doDestroy(irc_common_ctx_t* common_ctx, irc_ctx_t* ctx) {
	if ((ctx->state != STATE_CONNECTED)
			&& (ctx->state != STATE_UNCREATED)
			&& (ctx->state != STATE_WAIT_TO_RECONNECT)) {
		printf("Destroy connection to %s\n", server_conf_get_name(ctx->server_conf));
		irc_destroy_session(ctx->s);
		ctx->state = STATE_WAIT_TO_RECONNECT;
	}
	return 1;
}

int doWait(irc_common_ctx_t* common_ctx, irc_ctx_t* ctx) {
	if (ctx->state == STATE_WAIT_TO_RECONNECT) {
		fprintf(stderr, "W");

		struct timeval current;
		if (gettimeofday(&current, NULL)) {
			printf("ERROR: gettimeofday() : %s.\n", strerror(errno));
		}
		if (current.tv_sec > ctx->wait_date.tv_sec && current.tv_usec > ctx->wait_date.tv_usec) {
			ctx->state = STATE_UNCREATED;
			ctx->wait_date.tv_sec = 0;
			ctx->wait_date.tv_usec = 0;
		}
	}
	return 1;
}

void resetSelectData(irc_common_ctx_t* common_ctx) {
	common_ctx->tv.tv_sec = 1;
	common_ctx->tv.tv_usec = 0;
	common_ctx->maxfd = 0;
	FD_ZERO (&common_ctx->in_set);
	FD_ZERO (&common_ctx->out_set);
}

void SIGINThandler(int sig) {
	printf("Stopping program.\n");
	g_askedToStop = 1;
}

int main (int argc, char **argv) {
	irc_common_ctx_t common_ctx;

	memset (&common_ctx, 0, sizeof(irc_common_ctx_t));

	initCallbacks(&common_ctx.callbacks);

	// ----------

	common_ctx.irc_conf = irc_conf_new();
	if (!irc_conf_load(common_ctx.irc_conf, "testIrc.conf")) {
		irc_conf_free(common_ctx.irc_conf);
		return 1;
	}

	common_ctx.servers_count = irc_conf_get_servers_count(common_ctx.irc_conf);
	common_ctx.servers_ctx = calloc(common_ctx.servers_count, sizeof(irc_ctx_t));

	int eachServer;
	for (eachServer = 0; eachServer < common_ctx.servers_count; eachServer++) {
		irc_ctx_t * irc_ctx = &common_ctx.servers_ctx[eachServer];
		irc_ctx->state = STATE_UNCREATED;
		irc_ctx->server_conf = irc_conf_get_server_at(common_ctx.irc_conf, eachServer);
	}

	// ----------

	signal(SIGINT, SIGINThandler);

	// ----------

	while (!g_askedToStop) {
		doAction(&common_ctx, &doCreation);
		doAction(&common_ctx, &doConnection);

		resetSelectData(&common_ctx);
		doAction(&common_ctx, &doAddDescriptors);
		doSelect(&common_ctx);
		doAction(&common_ctx, &doProcessDescriptors);

		doAction(&common_ctx, &doDestroy);
		doAction(&common_ctx, &doWait);
	}

	irc_conf_free(common_ctx.irc_conf);
	free(common_ctx.servers_ctx);

	printf("End of program.\n");

	return 0;
}

