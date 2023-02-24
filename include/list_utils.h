#ifndef LIST_UTILS_H

#define LIST_UTILS_H

#include "server_utils.h"


/*
 * Structura pentru un nod din lista de conexiuni
 */
struct node_t {
	struct connection_t *conn;
	struct node_t *next;
};

/*
 * Returneaza un pointer la un nou obiect de tip struct node_t
 * care contine un pointer la conexiunea conn. Campul next
 * este intializat la NULL.
 * @param conn
 * conexiunea care trebuie adaugata la noul nod
 */
struct node_t *new_node(struct connection_t *conn);

/*
 * Adauga conexiunea conn in lista de conexiuni list.
 * Nu se verifica daca conexiunea exista deja in lista,
 * iar adaugarea se face pe principiul stivei pentru
 * simplitate.
 * @param list
 * lista de conexiuni
 * @param conn
 * conexiunea care trebuie adaugata
 */
void add_list(struct node_t **list, struct connection_t *conn);

/*
 * Cauta in lista de conexiuni (*list) conexiunea conn
 * si o elimina din lista daca exista. Se elibereaza resursele
 * aferente elementului sters din lista.
 * @param list
 * lista de conexiuni
 * @param conn
 * conexiunea care trebuie stearsa
 */
void remove_list(struct node_t **list, struct connection_t *conn);

/*
 * Afiseaza continutul unei liste. Functia a fost
 * folosita pentru debugging si nu este apelata in
 * program.
 * @param list
 * lista care este afisata
 */
void print_list(struct node_t *list);

/*
 * Se elibereaza resursele pentru
 * lista transmisa ca parametru
 * @param list
 * lista careia i se elibereaza resursele
 */
void free_list(struct node_t *list);

#endif