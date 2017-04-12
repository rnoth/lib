#ifndef _string_
#define _string_
#include <stdio.h>

#include "vec.h"

#ifndef isascii
# define isascii(C) (!((C) & (1<<7)))
#endif

#define eol(str, pos) (pos >= len(str))

typedef char string_t;

/* str.c */
void	str_chomp	(char *);
void	str_free	(string_t *);
string_t* str_alloc	(void);

/* str-io */
int	str_readline	(string_t **, FILE *);
int	str_write	(FILE *, string_t const *);

/* utf8.c */
int 	get_uchar	(char *, const char *);
int	uchar_extent	(const unsigned char);
size_t	ustrlen		(const char *);
#endif
