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
#include "../include/macros.h"
#include "../include/list_utils.h"
#include "../include/path_utils.h"
#include "../include/server_utils.h"
#include "../dependencies/http-parser/http_parser.h"

static struct epoll_event events[SOMAXCONN];

int main(int argc, char** argv)
{
	int nr_events;
	int res;
	struct epoll_event evt;
	char *port = getenv("ASYNC_SERVER_PORT");
	fprintf(stderr, "%s\n", port);
	if (port != NULL) {
		res = init_server(port);
	} else {
		res = init_server(AWS_PORT);
	}
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
