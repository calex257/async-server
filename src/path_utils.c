#include <stdio.h>
#include <sys/stat.h>
#include "../dependencies/http-parser/http_parser.h"
#include "../include/path_utils.h"
#include "../include/server_utils.h"

/*
 * Se sterge primul caracter '/' din path
 * @param path
 * path-ul care trebuie transformat
 */
void make_path_relative(char *path)
{
	char auxbuf[256] = { 0 };

	strcpy(auxbuf, path + 1);
	strcpy(path, auxbuf);
}

/*
 * Se transforma path-ul primit din request in path relativ
 * si se verifica daca fisierul cerut exista sau nu. Daca
 * exista, se extrage dimensiunea lui.
 * @param conn
 * pointer la structura aferenta conexiunii care este
 * tratata
 */
int check_path(struct connection_t *conn)
{
	struct stat st;
	int exists;

	make_path_relative(conn->req_file_path);
	exists = stat(conn->req_file_path, &st);
	if (exists < 0)
		return -1;
	conn->req_file_size = st.st_size;
	return 0;
}
