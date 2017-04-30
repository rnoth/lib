#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "util.h"

void
str_chomp(char *s)
{
	char *nl = 0;
	do if (*s == '\n') nl = s; while (*s++);
	*nl = 0;
}

void
str_free(string_t *str)
{
	vec_free(str);
}

string_t *
str_alloc(void)
{
	string_t *ret;
	vec_ctor(ret);
	return ret;
}
