#ifndef SERVER_UTILS_H

#define SERVER_UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <libaio.h>
#include <arpa/inet.h>
#include <sys/eventfd.h>
#include "list_utils.h"

struct connection_t {
	/*
	 * File descriptor-ul asociat socket-ului conexiunii
	 */
	int fd;
	/*
	 * Structura care retine adresa clientului conectat
	 */
	struct sockaddr address;
	/*
	 * Dimensiunea in bytes a campului address
	 */
	unsigned int addr_len;
	/*
	 * Enum pentru starile in care se poate afla o conexiune
	 */
	enum state {
		/*
		 * Conexiunea este nou-creata, urmeaza sa se primeasca date
		 */
		NEW = 0,
		/*
		 * Toate datele de la client au fost primite
		 */
		RECEIVED_FROM = 1,
		/*
		 * Toate datele au fost transmise la client
		 */
		SENT_TO = 2,
		/*
		 * Conexiunea este inchisa
		 */
		CLOSED = 3,
		/*
		 * Fisierul cerut este dinamic si trebuie
		 * citite date din el
		 */
		ASYNC_FREAD = 4,
		/*
		 * Fisierul cerut este dinamic si trebuie
		 * transmise datele citite anterior
		 */
		ASYNC_SEND = 5
	} state;
	/*
	 * Buffer-ul in care se primesc datele de la client
	 */
	char inbuf[BUFSIZ];
	/*
	 * Contor pentru bytes-ii cititi de la client
	 */
	size_t bytes_read;
	/*
	 * Buffer in care se stocheaza datele care trebuie
	 * transmise la client
	 */
	char outbuf[BUFSIZ];
	/*
	 * Retine numarul de bytes care trebuie scrisi
	 */
	size_t bytes_to_write;
	/*
	 * Contor pentru bytes-ii scrisi la un moment dat.
	 * Valoarea este identica cu bytes_written_total
	 * pentru fisierele statice, pentru fisierele
	 * dinamice se compara cu bytes_to_write pentru
	 * a sincroniza operatiile asincrone cu send-urile
	 */
	size_t bytes_written;
	/*
	 * Numarul total de bytes scrisi
	 */
	size_t bytes_written_total;
	/*
	 * Lungimea header-ului HTTP transmis ca parte din
	 * raspunsul pentru client
	 */
	int header_len;
	/*
	 * Variabila in care se retine daca conexiunea
	 * este pentru un fisier dinamic sau nu
	 */
	int is_async;
	/*
	 * Variabila in care se retine daca fisierul
	 * cerut exista sau nu
	 */
	int file_exists;
	/*
	 * Context pentru efectuarea operatiilor asincrone
	 * pe fisiere
	 */
	io_context_t io_con;
	/*
	 * Structura folosita pentru executarea unei operatii
	 * asincrone pe fisiere
	 */
	struct iocb *icb;
	/*
	 * Eventfd folosit pentru notificarea terminarii unei
	 * operatii asincrone pe fisiere
	 */
	eventfd_t evfd;
	/*
	 * Calea fisierului cerut de client
	 */
	char req_file_path[256];
	/*
	 * Dimensiunea fisierului cerut
	 */
	size_t req_file_size;
	/*
	 * File descriptor asociat fisierului cerut de client
	 */
	int req_file_fd;
};


/*
 * Structura care retine date pentru server,
 * relevante pentru toate conexiunile.
 */
struct server_t {
	/*
	 * Instanta de epoll aferenta serverului.
	 */
	int epoll_instance;
	/*
	 * File descriptor-ul asociat socket-ului de listen.
	 */
	int listener;
	/*
	 * Pointer la inceputul listei de conexiuni.
	 */
	struct node_t *conn_list;
	/*
	 * Pointer catre conexiunea care este tratata la un moment dat.
	 */
	struct connection_t *curr_connection;
} server;



/*
 * Returneaza un pointer la un nou obiect de tip struct connection_t
 * Se initializeaza campurile din structura cu valori initiale si se
 * creeaza un obiect de tip eventfd.
 */
struct connection_t *new_connection(void);

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
int get_listening_socket(char *ip_addr, char *port);

/*
 * Se elibereaza resursele aferente unei conexiuni
 * transmise ca parametru. Conexiunea se scoate din lista
 * de conexiuni a server-ului.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void remove_connection(struct connection_t *conn);

/*
 * Se accepta o noua conexiune pe socket-ul de listen si
 * se marcheaza ca non-blocanta. Se adauga noua conexiune
 * la lista de evenimente epoll, initial pentru evenimente
 * de tip EPOLLIN(input) si se adauga in lista de conexiuni
 * retinuta de server.
 */
void register_connection(void);

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
void parse_request(struct connection_t *conn, size_t res);

/*
 * Dupa ce request-ul a fost citit, se parseaza
 * request-ul primit si se verifica daca fisierul exista.
 * Pe baza acestei informatii, se configureaza header-ul
 * pentru raspunsul HTTP care trebuie trimis clientului.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void prepare_transfer(struct connection_t *conn);

/*
 * Transfera datele care compun header-ul HTTP al mesajului
 * care trebuie trimis clientului.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void transfer_header(struct connection_t *conn);

/*
 * Transfera date dintr-un fisier static folosind
 * mecanismul de zero copy. Numarul de bytes scrisi
 * este adunat la total
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void receive_async_data(struct connection_t *conn);

/*
 * Transfera date dintr-un fisier static folosind
 * mecanismul de zero copy. Numarul de bytes scrisi
 * este adunat la total
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void transfer_static_file(struct connection_t *conn);

/*
 * Transfera date din buffer-ul de output al conexiunii
 * in cel al socket-ului. Daca dupa operatia de scriere
 * este transmis tot fisierul, se elibereaza resursele
 * alocate pentru operatiile asincrone pe fisiere.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void transfer_dynamic_file(struct connection_t *conn);

/*
 * In functie de cat s-a transmis din raspunsul catre
 * client si tipul de fisier, se apeleaza functia potrivita
 * contextului. Daca totul a fost trimis, se inchide conexiunea
 * si se elibereaza resursele
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void transfer_data(struct connection_t *conn);

/*
 * Se citesc datele primite de la client pana la intalnirea
 * a doua newline-uri "\r\n\r\n", care marcheaza finalul
 * mesajului. Se actualizeaza in lista evenimentelor epoll
 * file descriptorul aferent conexiunii.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void read_request(struct connection_t *conn);

/*
 * Initializeaza serverul. Se creaza socket-ul de listen,
 * instanta de epoll si se initializeaza lista de conexiuni,
 * fiind initial vida. Se adauga la lista de file descriptori
 * a instantei de epoll socket-ul de listen.
 */
int init_server(char* port);

/*
 * Se primeste rezultatul citirii asincrone din fisier
 * si se actualizeaza lista de evenimente a instantei
 * de epoll pentru a marca faptul ca se pot trimite datele
 * citite la client.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
void send_async_data(struct connection_t *conn);

#endif