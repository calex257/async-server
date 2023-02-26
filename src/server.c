#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libaio.h>
#include <aio.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/eventfd.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <argp.h>
#include "../include/macros.h"
#include "../include/list_utils.h"
#include "../include/path_utils.h"
#include "../include/server_utils.h"
#include "../dependencies/http-parser/http_parser.h"

const char docstring[] = "Asynchronous Web Server - A basic"
"implementation of a server using I/O multiplexing";

const char *argp_program_version = "async_server 1.0";
static struct argp_option options[] = {
	{"port", 'p', "PORT", 0, "Select the port the server will run on", 0 },
	{ 0 }
};

struct arguments
{
	char *port;
};

static error_t parse_options (int key, char *arg, struct argp_state *state) {
	struct arguments *args = state->input;
	switch (key) {
		case 'p':
			args->port = arg;
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp parser = {options, parse_options, 0, docstring};

static struct epoll_event events[SOMAXCONN];

int main(int argc, char** argv)
{
	int nr_events;
	int res;
	struct epoll_event evt;
	struct arguments arguments;
	arguments.port = NULL;
	char *port = getenv("ASYNC_SERVER_PORT");
	if (port == NULL) {
		port = AWS_PORT;
	}
	argp_parse(&parser, argc, argv, 0, 0, &arguments);
	if (arguments.port != NULL) {
		port = arguments.port;
	}
	fprintf(stderr, "%s\n", port);
	res = init_server(port);
	if (res < 0)
		exit(1);
	memset(&evt, 0, sizeof(evt));
	while (true) {
		nr_events = epoll_wait(server.epoll_instance, events, sizeof(events) / sizeof(struct epoll_event), -1);
		if (nr_events == -1)
			perror(ERROR_STRING "epoll_wait");
		/*
		 * Se parcurge lista de evenimente returnata de epoll_wait
		 * si se trateaza fiecare eveniment in functie de tipul sau
		 * starea in care se afla conexiunea daca este cazul
		 */
		for (int i = 0; i < nr_events; i++) {
			if (events[i].data.fd == server.listener) {
				if (events[i].events & EPOLLIN)
					register_connection();
			} else {
				struct connection_t *conn = (struct connection_t *)events[i].data.ptr;

				if (events[i].events & EPOLLIN) {
					if (conn->state == ASYNC_FREAD)
						send_async_data(conn);
					else
						read_request(conn);
				} else if (events[i].events & EPOLLOUT)
					transfer_data(conn);
				else if (events[i].events & (EPOLLHUP | EPOLLRDHUP))
					remove_connection(conn);
			}
		}
	}
	close(server.listener);
	return 0;
}
