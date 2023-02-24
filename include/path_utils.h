#ifndef PATH_UTILS_H

#define PATH_UTILS_H

#include "../dependencies/http-parser/http_parser.h"
#include "server_utils.h"

/*
 * Se sterge primul caracter '/' din path
 * @param path
 * path-ul care trebuie transformat
 */
void make_path_relative(char *path);

/*
 * Se transforma path-ul primit din request in path relativ
 * si se verifica daca fisierul cerut exista sau nu. Daca
 * exista, se extrage dimensiunea lui.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
int check_path(struct connection_t *conn);

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
int get_http_request_path(http_parser *p, const char *buf, size_t len);

#endif