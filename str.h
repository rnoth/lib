#ifndef _string_
#define _string_
#include <stdio.h>

#include "vec.h"

#ifndef isascii
# define isascii(c) (!((c) & (1<<7)))
#endif

#define str_eol(STR, POS) (POS >= vec_len(STR))

/* str.c */
char * str_alloc        (void);
int    str_append       (char **, char);
void   str_chomp        (char **);
void   str_free         (char *);
int    str_readline     (char **, FILE *);

#endif
