#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "libircclient.h"

typedef struct {
	char * channel;
	char * nick;
} irc_ctx_t;

void addlog(const char * fmt, ...) {
	char buf[1024];
	va_list va_alist;

	va_start(va_alist, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va_alist);
	va_end(va_alist);

	printf("%s\n", buf);
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

	char text[256];
	sprintf (text, "Hi!");
	irc_cmd_msg (session, "Toff", text);

	irc_cmd_join (session, ctx->channel, 0);
}


void event_channel (irc_session_t * session, const char * event, const char * origin, const char ** params, unsigned int count)
{
	dump_event(session, event, origin, params, count);

	char nickbuf[128];

	if ( count != 2 )
		return;

	printf ("'%s' said in channel %s: %s\n",
		origin ? origin : "someone",
		params[0], params[1] );
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

int main (int argc, char **argv) {
	irc_callbacks_t	callbacks;
	irc_ctx_t ctx;
	irc_session_t * s;

	if (argc != 4) {
		printf("Usage: %s <server> <nick> <channel>\n", argv[0]);
		return 1;
	}

	memset (&callbacks, 0, sizeof(callbacks));

	callbacks.event_connect = event_connect;
	callbacks.event_channel = event_channel;

	callbacks.event_join = dump_event;
	callbacks.event_quit = dump_event;
	callbacks.event_part = dump_event;

	callbacks.event_nick = dump_event;
	callbacks.event_mode = dump_event;
	callbacks.event_topic = dump_event;
	callbacks.event_kick = dump_event;
	callbacks.event_privmsg = dump_event;
	callbacks.event_notice = dump_event;
	callbacks.event_invite = dump_event;
	callbacks.event_umode = dump_event;
	callbacks.event_ctcp_req = dump_event;
	callbacks.event_ctcp_rep = dump_event;
	callbacks.event_ctcp_action = dump_event;
	callbacks.event_unknown = dump_event;
	callbacks.event_numeric = event_numeric;



    int keepConnection = 1;
	while (keepConnection) {

		struct timeval tv = { 0, 250000 };
		int maxfd = 0;
		fd_set in_set, out_set;
		FD_ZERO (&in_set);
		FD_ZERO (&out_set);


		s = irc_create_session (&callbacks);
		if (!s) {
			printf("Could not create session\n");
			return 1;
		}

		ctx.channel = argv[3];
	    ctx.nick = argv[2];
	    irc_set_ctx (s, &ctx);
		if (irc_connect(s, argv[1], 6667, 0, argv[2], 0, 0)) {
			printf("Could not connect: %s\n", irc_strerror(irc_errno(s)));
			return 1;
		}


		while ( irc_is_connected(s) )
		{
			struct timeval tv;
			fd_set in_set, out_set;
			int maxfd = 0;

			tv.tv_sec = 0;
			tv.tv_usec = 250000;

			// Init sets
			FD_ZERO (&in_set);
			FD_ZERO (&out_set);

			irc_add_select_descriptors (s, &in_set, &out_set, &maxfd);

			int retval = select (maxfd + 1, &in_set, &out_set, 0, &tv);
			if ( retval < 0 )
			{
				if ( errno == EINTR ) {
					fprintf(stderr, "I");
					continue;
				}
				printf("Select error.\n");
				break;
			} else if (retval == 0) {
				fprintf(stderr, "T");
			}

			if ( irc_process_select_descriptors (s, &in_set, &out_set) ) {
				printf("irc_process_select_descriptors() error: %s (%d)\n",
						irc_strerror(irc_errno(s)), irc_errno(s));
				break;
			}
		}

		//irc_run(s);


		irc_destroy_session(s);

		printf("Connection with server was closed, sleep 2s before connecting again.\n");
		sleep(2);
	}

	return 0;
}
