#ifndef CONF_H_
#define CONF_H_

// Use opaque pointer : http://en.wikipedia.org/wiki/Opaque_pointer
typedef struct _regex_conf_t regex_conf_t;
typedef struct _filter_conf_t filter_conf_t;
typedef struct _cmd_conf_t cmd_conf_t;
typedef struct _channel_conf_t channel_conf_t;
typedef struct _server_conf_t server_conf_t;
typedef struct _irc_conf_t irc_conf_t;


// ----- regex_conf

const char * regex_conf_get_regex(regex_conf_t * regex_conf);

int regex_conf_get_vars_count(regex_conf_t * regex_conf);

const char * regex_conf_get_var_at(regex_conf_t * regex_conf, int index);

// ----- filter_conf

const char * filter_conf_get_name(filter_conf_t * filter_conf);

int filter_conf_get_regexes_count(filter_conf_t * filter_conf);

regex_conf_t * filter_conf_get_regex_at(filter_conf_t * filter_conf, int index);

// ---- cmd_conf

const char * cmd_conf_get_name(cmd_conf_t * cmd_conf);

const char * cmd_conf_get_arg1(cmd_conf_t * cmd_conf);

const char * cmd_conf_get_arg2(cmd_conf_t * cmd_conf);

// ---- channel_conf

const char * channel_conf_get_name(channel_conf_t * channel_conf);

const char * channel_conf_get_passwd(channel_conf_t * channel_conf);

const char * channel_conf_get_nickfilter(channel_conf_t * channel_conf);

int channel_conf_get_filters_count(channel_conf_t * channel_conf);

filter_conf_t * channel_conf_get_filter_at(channel_conf_t * channel_conf, int index);

// ----- server_conf

const char * server_conf_get_name(server_conf_t * server_conf);

const char * server_conf_get_ip(server_conf_t * server_conf);

int server_conf_get_port(server_conf_t * server_conf);

const char * server_conf_get_passwd(server_conf_t * server_conf);

const char * server_conf_get_nick(server_conf_t * server_conf);

int server_conf_get_channels_count(server_conf_t * server_conf);

channel_conf_t * server_conf_get_channel_at(server_conf_t * server_conf, int index);

int server_conf_get_cmds_count(server_conf_t * server_conf);

cmd_conf_t * server_conf_get_cmd_at(server_conf_t * server_conf, int index);

// ----- irc_conf

irc_conf_t * irc_conf_new();

void irc_conf_free(irc_conf_t * irc_conf);

int irc_conf_load(irc_conf_t * irc_conf, const char* filename);

int irc_conf_get_servers_count(irc_conf_t * irc_conf);

server_conf_t * irc_conf_get_server_at(irc_conf_t * irc_conf, int index);

// -----


#endif /* CONF_H_ */
