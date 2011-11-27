#include <string.h>
#include <jansson.h>

#include "conf.h"

/*
 * servers[]/@{ip,port,nick,password}
 *          /cmds[]/@{name,arg1,arg2}
 *          /channels[]/@{name,password,nickfilter}
 */


// ---

struct _regex_conf_t {
    json_t * regex_node;
    json_t * vars_node;
};

struct _filter_conf_t {
    json_t * filter_node;
    json_t * regexes_node;
    regex_conf_t current_regex;
};

struct _channel_conf_t {
//     char *name;
//     char *passwd;

    json_t * node;
    json_t * filters_node;
    filter_conf_t current_filter;
};

struct _cmd_conf_t {
//     char* cmd;
//     char* arg1;
//     char* arg2;

    json_t * node;
};

struct _server_conf_t {
//     char * name;
//     char * ip;
//     int port;
//     char * passwd;
//     char * nick;
//     channel_conf_t * channels;
//     int channels_count;
//     cmd_t * cmds;
//     int cmds_count;

    json_t * node;
    json_t * channels_node;
    json_t * cmds_node;

    channel_conf_t current_channel;
    cmd_conf_t current_cmd;
};

struct _irc_conf_t {
	json_t * root;
    json_t * servers_root;
    server_conf_t current_server;
};


// ----- regex_conf

const char * regex_conf_get_regex(regex_conf_t * regex_conf) {
    json_t * jValue = json_object_get(regex_conf->regex_node, "regex");
    return json_string_value(jValue);
}

int regex_conf_get_vars_count(regex_conf_t * regex_conf) {
    return json_array_size(regex_conf->vars_node);
}

const char * regex_conf_get_var_at(regex_conf_t * regex_conf, int index) {
    json_t * jValue = json_array_get(regex_conf->vars_node, index);
    return json_string_value(jValue);
}

// ----- filter_conf

const char * filter_conf_get_name(filter_conf_t * filter_conf) {
    json_t * jValue = json_object_get(filter_conf->filter_node, "name");
    return json_string_value(jValue);
}

int filter_conf_get_regexes_count(filter_conf_t * filter_conf) {
    return json_array_size(filter_conf->regexes_node);
}

regex_conf_t * filter_conf_get_regex_at(filter_conf_t * filter_conf, int index) {
    json_t * regex_node = json_array_get(filter_conf->regexes_node, index);
    filter_conf->current_regex.regex_node = regex_node;
    filter_conf->current_regex.vars_node = json_object_get(regex_node, "vars");
    return &filter_conf->current_regex;
}

// ----- cmd_conf

const char * cmd_conf_get_name(cmd_conf_t * cmd_conf) {
    json_t *jValue = json_object_get(cmd_conf->node, "name");
    if (!json_is_string(jValue)) {
        fprintf(stderr, "ERROR: cmd_conf_get_name() : could not find command 'name' or it's not a string.\n",
            cmd_conf, cmd_conf->node);
    }
    return json_string_value(jValue);
}

const char * cmd_conf_get_arg1(cmd_conf_t * cmd_conf) {
    json_t *jValue = json_object_get(cmd_conf->node, "arg1");
    return json_string_value(jValue);
}

const char * cmd_conf_get_arg2(cmd_conf_t * cmd_conf) {
    json_t *jValue = json_object_get(cmd_conf->node, "arg2");
    return json_string_value(jValue);
}

// ---- channel_conf

const char * channel_conf_get_name(channel_conf_t * channel_conf) {
    json_t *jValue = json_object_get(channel_conf->node, "name");
    return json_string_value(jValue);
}

const char * channel_conf_get_passwd(channel_conf_t * channel_conf) {
    json_t *jValue = json_object_get(channel_conf->node, "passwd");
    return json_string_value(jValue);
}

const char * channel_conf_get_nickfilter(channel_conf_t * channel_conf) {
    json_t *jValue = json_object_get(channel_conf->node, "nickfilter");
    return json_string_value(jValue);
}

int channel_conf_get_filters_count(channel_conf_t * channel_conf) {
    if (!json_is_array(channel_conf->filters_node)) {
    	fprintf(stderr, "ERROR: filters is not an array.\n");
        return 0;
    }
    return json_array_size(channel_conf->filters_node);
}

filter_conf_t * channel_conf_get_filter_at(channel_conf_t * channel_conf, int index) {
	json_t * filter_node = json_array_get(channel_conf->filters_node, index);;
	channel_conf->current_filter.filter_node = filter_node;
	channel_conf->current_filter.regexes_node = json_object_get(filter_node, "regexes");
    return &channel_conf->current_filter;
}

// -----

const char * server_conf_get_name(server_conf_t * server_conf) {
    json_t *jValue = json_object_get(server_conf->node, "name");
    return json_string_value(jValue);
}

const char * server_conf_get_ip(server_conf_t * server_conf) {
    json_t *jValue = json_object_get(server_conf->node, "ip");
    return json_string_value(jValue);
}

int server_conf_get_port(server_conf_t * server_conf) {
    json_t *jValue = json_object_get(server_conf->node, "port");
    return json_integer_value(jValue);
}

const char * server_conf_get_passwd(server_conf_t * server_conf) {
    json_t *jValue = json_object_get(server_conf->node, "passwd");
    return json_string_value(jValue);
}

const char * server_conf_get_nick(server_conf_t * server_conf) {
    json_t *jValue = json_object_get(server_conf->node, "nick");
    return json_string_value(jValue);
}

int server_conf_get_channels_count(server_conf_t * server_conf) {
    return json_array_size(server_conf->channels_node);
}

channel_conf_t * server_conf_get_channel_at(server_conf_t * server_conf, int index) {
	json_t * channel_node = json_array_get(server_conf->channels_node, index);
    if (!json_is_object(channel_node)) {
        fprintf(stderr, "ERROR: channels[%d] is not an object.\n", index);
    }
    server_conf->current_channel.node = channel_node;
    server_conf->current_channel.filters_node = json_object_get(channel_node, "filters");
    return &server_conf->current_channel;
}

int server_conf_get_cmds_count(server_conf_t * server_conf) {
    if (!json_is_array(server_conf->cmds_node)) {
    	fprintf(stderr, "ERROR: cmds is not an array.\n");
        return 0;
    }
    return json_array_size(server_conf->cmds_node);
}

cmd_conf_t * server_conf_get_cmd_at(server_conf_t * server_conf, int index) {
    json_t* cmdNode = json_array_get(server_conf->cmds_node, index);
    if (!json_is_object(cmdNode)) {
        fprintf(stderr, "ERROR: cmds[%d] is not an object.\n", index);
    }
    server_conf->current_cmd.node = cmdNode;
    return &server_conf->current_cmd;
}

// -----

irc_conf_t * irc_conf_new() {
	irc_conf_t * result = calloc(1, sizeof(struct _irc_conf_t));
	return result;
}

void irc_conf_free(irc_conf_t * irc_conf) {
	if (irc_conf->root) {
		json_decref(irc_conf->root);
	}
	memset(irc_conf, 0, sizeof(struct _irc_conf_t));
	free(irc_conf);
}

int validate(json_t * root) {
    json_error_t error;

    // Do a first check for mandatory fields
    if (json_unpack_ex(root, &error, JSON_VALIDATE_ONLY,
            "{ s:[ \
                {s:s, s:s, s:i, s:s, \
                s:[ \
                        {s:s}] \
                }]\
            }",
            "servers",
            "name", "ip", "port", "nick",
            "channels",
            "name") < 0) {
        fprintf(stderr, "error: on line %d column %d: %s\n", error.line, error.column, error.text);
        return 0;
    }

    // Check for mandatory fields in optional objects
    json_t *servers = json_object_get(root, "servers");
    int eachServer;
    for (eachServer = 0; eachServer < json_array_size(servers); eachServer++) {
        json_t *server = json_array_get(servers, eachServer);
        json_t *cmds = json_object_get(server, "cmds");
        if (cmds != NULL) {
            if (json_unpack_ex(cmds, &error, JSON_VALIDATE_ONLY, "[{s:s}]", "name") < 0) {
                fprintf(stderr, "error %s on node cmds: on line %d column %d: %s\n", error.source, error.line, error.column, error.text);
                return 0;
            }
            int eachCmd;
            for (eachCmd = 0; eachCmd < json_array_size(cmds); eachCmd++) {
                json_t *cmd = json_array_get(cmds, eachCmd);
                json_t *cmdObj = json_object_get(cmd, "name");
                const char *cname = json_string_value(cmdObj);
                if (strcmp(cname, "msg") == 0) {
                    // we need 2 args (nick, text)
                    if (json_unpack_ex(cmd, &error, JSON_VALIDATE_ONLY,"{s:s, s:s}", "arg1", "arg2") < 0) {
                        fprintf(stderr, "error on node cmds[%d]: on line %d column %d: %s\n", eachCmd, error.line, error.column, error.text);
                        return 0;
                    }
                } else {
                    fprintf(stderr, "error on node cmds[%d]: on line %d column %d: %s\n", eachCmd, error.line, error.column, error.text);
                    return 0;
                }
            }
        }
    }

    return 1;
}


int irc_conf_load(irc_conf_t * irc_conf, const char* filename) {
	json_error_t error;

	irc_conf->root = json_load_file(filename, 0, &error);
	if (!irc_conf->root) {
	    fprintf(stderr, "error: on line %d column %d: %s\n", error.line, error.column, error.text);
	    return 0;
	}

	if (!validate(irc_conf->root)) {
	    return 0;
	}

    json_t *servers = json_object_get(irc_conf->root, "servers");
    irc_conf->servers_root = servers;
    return 1;
}

int irc_conf_get_servers_count(irc_conf_t * irc_conf) {
    return json_array_size(irc_conf->servers_root);
}

server_conf_t * irc_conf_get_server_at(irc_conf_t * irc_conf, int index) {
    json_t * server_node = json_array_get(irc_conf->servers_root, index);
    irc_conf->current_server.node = server_node;
    irc_conf->current_server.channels_node = json_object_get(server_node, "channels");
    irc_conf->current_server.cmds_node = json_object_get(server_node, "cmds");
    return &irc_conf->current_server;
}

// -----
