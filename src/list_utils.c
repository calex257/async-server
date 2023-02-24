#include <stdio.h>
#include <stdlib.h>
#include "../include/list_utils.h"
#include "../include/server_utils.h"

/*
 * Returneaza un pointer la un nou obiect de tip struct node_t
 * care contine un pointer la conexiunea conn. Campul next
 * este intializat la NULL.
 * @param conn
 * conexiunea care trebuie adaugata la noul nod
 */
struct node_t *new_node(struct connection_t *conn)
{
	struct node_t *nd = malloc(sizeof(*nd));

	nd->next = NULL;
	nd->conn = conn;
	return nd;
}

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
void add_list(struct node_t **list, struct connection_t *conn)
{
	if ((*list) == NULL) {
		(*list) = new_node(conn);
		return;
	}
	struct node_t *p = new_node(conn);

	p->next = (*list);
	(*list) = p;
}

/*
 * Cauta in lista de conexiuni (*list) conexiunea conn
 * si o elimina din lista daca exista. Se elibereaza resursele
 * aferente elementului sters din lista.
 * @param list
 * lista de conexiuni
 * @param conn
 * conexiunea care trebuie stearsa
 */
void remove_list(struct node_t **list, struct connection_t *conn)
{
	if ((*list) == NULL)
		return;
	if ((*list)->conn == conn) {
		struct node_t *p = (*list);
		(*list) = (*list)->next;
		free(p->conn);
		free(p);
		return;
	}
	struct node_t *p = (*list), *q;

	while (p->next != NULL && p->next->conn != conn)
		p = p->next;
	if (p->next == NULL)
		return;
	q = p->next;
	p->next = q->next;
	free(q->conn);
	free(q);
}

/*
 * Afiseaza continutul unei liste. Functia a fost
 * folosita pentru debugging si nu este apelata in
 * program.
 * @param list
 * lista care este afisata
 */
void print_list(struct node_t *list)
{
	while (list != NULL) {
		fprintf(stderr, "%x\n", list->conn->fd);
		list = list->next;
	}
}

/*
 * Se elibereaza resursele pentru
 * lista transmisa ca parametru
 * @param list
 * lista careia i se elibereaza resursele
 */
void free_list(struct node_t *list)
{
	struct node_t *p = list, *q = p;

	while (p != NULL) {
		q = p->next;
		free(p->conn);
		free(p);
		p = q;
	}
}
