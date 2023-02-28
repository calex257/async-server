#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <libaio.h>
#include <aio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/sendfile.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include "../include/macros.h"
#include "../include/path_utils.h"
#include "../dependencies/http-parser/http_parser.h"
#include "../include/server_utils.h"


/*
 * Template folosit pentru un header HTTP pentru codul 404
 */
static const char bad_juju[] = "HTTP/1.1 404 Not Found\r\n"
"Content-Length: 0\r\n"
"Connection: close\r\n";

/*
 * Template folosit pentru un header HTTP pentru codul 200
 */
static const char *request_boilerplate = "HTTP/1.1 200 OK\r\n"
"Date: Wed, March 24, 1999 4:20:25 AM\r\n"
"Server: The Matrix/19.9.9\r\n"
"Connection: close\r\n"
"Content-Type: text/html\r\n"
"Content-Length: %u\r\n"
"\r\n";

static char path_buffer[256];

/*
 * Structura care contine callback-urile transmise
 * unui parser HTTP. Din moment ce singura valoare
 * relevanta pentru acest server este path-ul, se
 * seteaza un callback non-NULL doar pentru on_path.
 */
static struct http_parser_settings settings = {
	.on_message_begin = 0,
	.on_path = get_http_request_path,
	.on_query_string = 0,
	.on_url = 0,
	.on_fragment = 0,
	.on_header_field = 0,
	.on_header_value = 0,
	.on_headers_complete = 0,
	.on_body = 0,
	.on_message_complete = 0
};

/*
 * Callback folosit pentru obtinerea path-ului din
 * request-ul primit de la client.
 * @param p
 * parser-ul care apeleaza callback-ul
 * @param buf
 * buffer-ul din care se transfera path-ul
 * @param len
 * numarul de bytes care trebuie transferati
 */
int get_http_request_path(http_parser *p, const char *buf, size_t len)
{
	strncat(path_buffer, buf, len);
	return 0;
}

/*
 * Returneaza un pointer la un nou obiect de tip struct connection_t
 * Se initializeaza campurile din structura cu valori initiale si se
 * creeaza un obiect de tip eventfd.
 */
struct connection_t *new_connection(void)
{
	struct connection_t *conn;

	conn = malloc(sizeof(*conn));
	conn->state = NEW;
	conn->bytes_read = 0;
	conn->bytes_written = 0;
	conn->bytes_written_total = 0;
	conn->addr_len = sizeof(conn->address);
	conn->evfd = eventfd(0, O_NONBLOCK);
	if (conn->evfd < 0)
		perror(ERROR_STRING "eventfd");
	memset(conn->inbuf, 0, BUFSIZ);
	memset(conn->req_file_path, 0, 256);
	return conn;
}

/*
 * Initializeaza serverul. Se creaza socket-ul de listen,
 * instanta de epoll si se initializeaza lista de conexiuni,
 * fiind initial vida. Se adauga la lista de file descriptori
 * a instantei de epoll socket-ul de listen.
 */
int init_server(struct server_t* server, char* port)
{
	server->listener = get_listening_socket(LOCALHOST, port);
	server->epoll_instance = epoll_create1(0);
	if (server->epoll_instance < 0) {
		perror(ERROR_STRING "epoll_create1");
		return -1;
	}
	server->conn_list = NULL;
	struct epoll_event evt;

	memset(&evt, 0, sizeof(evt));
	evt.data.fd = server->listener;
	evt.events = EPOLLIN;
	D_CALL(epoll_ctl(server->epoll_instance, EPOLL_CTL_ADD, server->listener, &evt), < 0);
	return 0;
}

/*
 * Se creeaza socket-ul de listen si i se asigneaza
 * adresa ip_addr si portul port, daca este posibil.
 * ip_addr trebuie transmis in format IPv4
 * (e.g. "123.123.123.123"), iar portul transmis ca
 * sir de caractere.
 * @param ip_addr
 * adresa dorita pentru socket-ul de listen
 * @param port
 * portul dorit pentru socket-ul de listen
 */
int get_listening_socket(char *ip_addr, char *port)
{
	struct addrinfo hints, *results;
	int yes = 1, sock;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;

	sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (sock < 0) {
		perror(ERROR_STRING "socket");
		exit(-1);
	}
	D_CALL(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)), < 0);
	D_CALL_EXIT(getaddrinfo(ip_addr, port, &hints, &results), < 0);
	D_CALL_EXIT(bind(sock, results->ai_addr, results->ai_addrlen), < 0);
	D_CALL_EXIT(listen(sock, SOMAXCONN), < 0);
	freeaddrinfo(results);
	return sock;
}

/*
 * Se elibereaza resursele aferente unei conexiuni
 * transmise ca parametru. Conexiunea se scoate din lista
 * de conexiuni a server-ului.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void remove_connection(struct server_t *server, struct connection_t *conn)
{
	if (conn->file_exists)
		D_CALL_EXIT(close(conn->req_file_fd), < 0);
	D_CALL(epoll_ctl(server->epoll_instance, EPOLL_CTL_DEL, conn->fd, NULL), < 0);
	D_CALL_EXIT(close(conn->fd), < 0);
	remove_list(&(server->conn_list), conn);
}

/*
 * Se accepta o noua conexiune pe socket-ul de listen si
 * se marcheaza ca non-blocanta. Se adauga noua conexiune
 * la lista de evenimente epoll, initial pentru evenimente
 * de tip EPOLLIN(input) si se adauga in lista de conexiuni
 * retinuta de server->
 */
void register_connection(struct server_t *server)
{
	struct epoll_event evt;
	struct connection_t *conn = new_connection();
	int flags;

	memset(&evt, 0, sizeof(evt));
	conn->fd = accept4(server->listener, &(conn->address), &(conn->addr_len), SOCK_NONBLOCK);
	flags = fcntl(conn->fd, F_GETFL);
	D_CALL(fcntl(conn->fd, F_SETFL, flags | O_NONBLOCK), < 0);
	CHECK_NON_BLOCK(conn->fd);
	evt.data.ptr = conn;
	evt.events = EPOLLIN | EPOLLRDHUP;
	conn->state = NEW;
	D_CALL(epoll_ctl(server->epoll_instance, EPOLL_CTL_ADD, conn->fd, &evt), < 0);
	add_list(&(server->conn_list), conn);
}

/*
 * Dupa ce request-ul a fost citit, se parseaza
 * request-ul primit si se verifica daca fisierul exista.
 * Pe baza acestei informatii, se configureaza header-ul
 * pentru raspunsul HTTP care trebuie trimis clientului.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void prepare_transfer(struct server_t *server, struct connection_t *conn)
{
	parse_request(server, conn, conn->bytes_read);
	int file_exists = check_path(conn);

	if (file_exists < 0) {
		conn->file_exists = 0;
		strcpy(conn->outbuf, bad_juju);
		conn->header_len = strlen(conn->outbuf);
	} else {
		conn->file_exists = 1;
		if (strstr(conn->req_file_path, "static") != NULL)
			conn->is_async = 0;
		else {
			conn->is_async = 1;
			conn->state = ASYNC_SEND;
		}
		sprintf(conn->outbuf, request_boilerplate, conn->req_file_size);
		conn->header_len = strlen(conn->outbuf);
		conn->req_file_fd = open(conn->req_file_path, O_RDONLY | O_NONBLOCK);
		if (conn->req_file_fd < 0)
			perror(ERROR_STRING "open");
	}
}

/*
 * Transfera datele care compun header-ul HTTP al mesajului
 * care trebuie trimis clientului.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void transfer_header(struct server_t *server, struct connection_t *conn)
{
	int res = send(conn->fd, conn->outbuf + conn->bytes_written,
		conn->header_len - conn->bytes_written, MSG_DONTWAIT);
	if (res > 0) {
		conn->bytes_written += res;
		conn->bytes_written_total += res;
		/*
		 * Daca s-a transmis tot header-ul si fisierul este dinamic
		 * se pregateste prima operatie asincrona de read pe fisierul
		 * cerut de client
		 */
		if (conn->bytes_written >= conn->header_len && conn->is_async) {
			conn->state = ASYNC_FREAD;
			struct epoll_event evt;

			evt.events = EPOLLIN | EPOLLET;
			evt.data.ptr = conn;
			D_CALL(epoll_ctl(server->epoll_instance, EPOLL_CTL_ADD, conn->evfd, &evt), < 0);
			conn->bytes_written = 0;
			io_setup(1, &(conn->io_con));
			conn->icb = calloc(1, sizeof(*(conn->icb)));
			io_prep_pread(conn->icb, conn->req_file_fd, conn->outbuf, BUFSIZ, 0);
			io_set_eventfd(conn->icb, conn->evfd);
			io_submit(conn->io_con, 1, &(conn->icb));
		}
	} else
		perror(ERROR_STRING "send");
}

/*
 * Se creeaza un parser HTTP care sa parseze request-ul
 * primit de la client. Path-ul rezultat se gaseste in
 * campul req_file_path al conexiunii.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 * @param res
 * numarul total de bytes cititi de la client
 */
void parse_request(struct server_t *server, struct connection_t *conn, size_t res)
{
	struct http_parser parser;

	server->curr_connection = conn;
	http_parser_init(&parser, HTTP_REQUEST);
	http_parser_execute(&parser, &settings, conn->inbuf, res);
	http_parser_execute(&parser, &settings, NULL, 0);
	strncpy(server->curr_connection->req_file_path, path_buffer, 256);
}

/*
 * Transfera date dintr-un fisier static folosind
 * mecanismul de zero copy. Numarul de bytes scrisi
 * este adunat la total
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void receive_async_data(struct server_t *server, struct connection_t *conn)
{
	struct epoll_event evt;

	conn->state = ASYNC_FREAD;
	evt.events = EPOLLIN | EPOLLET;
	evt.data.ptr = conn;
	io_prep_pread(conn->icb, conn->req_file_fd, conn->outbuf,
		BUFSIZ, conn->bytes_written_total - conn->header_len);
	io_set_eventfd(conn->icb, conn->evfd);
	CHECK_IO_SUBMIT_USE(io_submit(conn->io_con, 1, &(conn->icb)));
	D_CALL(epoll_ctl(server->epoll_instance, EPOLL_CTL_MOD, conn->evfd, &evt), < 0);
	D_CALL(epoll_ctl(server->epoll_instance, EPOLL_CTL_DEL, conn->fd, &evt), < 0);
}

/*
 * Transfera date dintr-un fisier static folosind
 * mecanismul de zero copy. Numarul de bytes scrisi
 * este adunat la total
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void transfer_static_file(struct connection_t *conn)
{
	int res = sendfile(conn->fd, conn->req_file_fd, NULL, conn->req_file_size);

	if (res > 0) {
		conn->bytes_written += res;
		conn->bytes_written_total += res;
	} else
		perror(ERROR_STRING "sendfile");
}

/*
 * Transfera date din buffer-ul de output al conexiunii
 * in cel al socket-ului. Daca dupa operatia de scriere
 * este transmis tot fisierul, se elibereaza resursele
 * alocate pentru operatiile asincrone pe fisiere.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void transfer_dynamic_file(struct server_t *server, struct connection_t *conn)
{
	if (conn->bytes_to_write - conn->bytes_written == 0) {
		conn->state = ASYNC_FREAD;
		receive_async_data(server, conn);
		return;
	}
	int res = send(conn->fd, conn->outbuf + conn->bytes_written, conn->bytes_to_write - conn->bytes_written, MSG_DONTWAIT);

	if (res > 0) {
		conn->bytes_written += res;
		conn->bytes_written_total += res;
		if (conn->bytes_written_total >= conn->header_len + conn->req_file_size) {
			close(conn->evfd);
			io_destroy(conn->io_con);
			free(conn->icb);
			return;
		}
		if (conn->bytes_written >= conn->bytes_to_write) {
			conn->state = ASYNC_FREAD;
			receive_async_data(server, conn);
		}
	} else
		perror(ERROR_STRING "send");
}

/*
 * In functie de cat s-a transmis din raspunsul catre
 * client si tipul de fisier, se apeleaza functia potrivita
 * contextului. Daca totul a fost trimis, se inchide conexiunea
 * si se elibereaza resursele
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void transfer_data(struct server_t *server, struct connection_t *conn)
{
	if (conn->bytes_written_total < conn->header_len)
		transfer_header(server, conn);
	if (conn->bytes_written_total >= conn->header_len && conn->file_exists) {
		if (conn->is_async)
			transfer_dynamic_file(server, conn);
		else
			transfer_static_file(conn);
	}
	if (conn->bytes_written_total >= conn->req_file_size + conn->header_len)
		remove_connection(server, conn);
}

/*
 * Se citesc datele primite de la client pana la intalnirea
 * a doua newline-uri "\r\n\r\n", care marcheaza finalul
 * mesajului. Se actualizeaza in lista evenimentelor epoll
 * file descriptorul aferent conexiunii.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void read_request(struct server_t *server, struct connection_t *conn)
{
	int res = recv(conn->fd, conn->inbuf + conn->bytes_read, BUFSIZ, MSG_DONTWAIT);

	if (res > 0) {
		conn->bytes_read += res;
		if (strstr(conn->inbuf, "\r\n\r\n") != NULL) {
			conn->state = RECEIVED_FROM;
			struct epoll_event new_e;

			new_e.events = EPOLLOUT | EPOLLRDHUP;
			new_e.data.ptr = conn;
			D_CALL(epoll_ctl(server->epoll_instance, EPOLL_CTL_MOD, conn->fd, &new_e), < 0);
			prepare_transfer(server, conn);
		}
		return;
	}
	struct epoll_event new_e;

	new_e.events = EPOLLRDHUP;
	new_e.data.ptr = conn;
	D_CALL(epoll_ctl(server->epoll_instance, EPOLL_CTL_MOD, conn->fd, &new_e), < 0);
}

/*
 * Se primeste rezultatul citirii asincrone din fisier
 * si se actualizeaza lista de evenimente a instantei
 * de epoll pentru a marca faptul ca se pot trimite datele
 * citite la client.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void send_async_data(struct server_t *server, struct connection_t *conn)
{
	struct epoll_event evt;
	struct io_event ievt;

	conn->state = ASYNC_SEND;
	evt.events = EPOLLOUT;
	evt.data.ptr = conn;
	io_getevents(conn->io_con, 1, 1, &ievt, NULL);
	conn->bytes_to_write = ievt.res;
	D_CALL(epoll_ctl(server->epoll_instance, EPOLL_CTL_ADD, conn->fd, &evt), < 0);
	conn->bytes_written = 0;
}