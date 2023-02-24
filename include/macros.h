#define true 1
#define false 0
#define AUXSTR(x) #x
#define STR(x) AUXSTR(x)
#define ERROR_STRING "@" __FILE__ " at line #" STR(__LINE__) "\n"
#define LOCALHOST "127.0.0.1"
// TODO
#define AWS_PORT "8888"

#define D_CALL_EXIT(func, cond) do {\
	int res = func;					\
	if (res cond) {					\
		perror(ERROR_STRING #func);	\
		exit(1);					\
	}								\
} while (0)

/*
 * Macro pentru verificarea return code-ului apelurilor de sistem
 */
#ifdef DEBUG
#define D_CALL(func, cond) do {		\
	int res = func;					\
	if (res cond) {					\
		perror(ERROR_STRING #func);	\
	}								\
} while (0)
#else
#define D_CALL(func, cond)			\
	func
#endif

/*
 * Alte macro-uri utile/pentru debugging
 */
#define true 1
#define false 0
#define AUXSTR(x) #x
#define STR(x) AUXSTR(x)
#define ERROR_STRING "@" __FILE__ " at line #" STR(__LINE__) "\n"
#define DEBUG
// #define SANITY_CHECK

#ifdef SANITY_CHECK
#define CHECK_NON_BLOCK(fd) do {			\
	int flags = fcntl(fd, F_GETFL);			\
	if (flags & O_NONBLOCK) {				\
		fprintf(stderr,						\
		"Socket marked as non_blocking\n");	\
	}										\
} while (0)

#define CHECK_IO_SUBMIT_USE(func) do {			\
	func;										\
	fprintf(stderr, #func " has been used\n");	\
} while (0)
#else
#define CHECK_NON_BLOCK(fd)
#define CHECK_IO_SUBMIT_USE(func) func
#endif
