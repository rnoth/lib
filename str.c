#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"
#include "util.h"
#include "vec.h"

int
str_append(char **dst, char src)
{
	int err = 0;
	size_t avail = 0;

	avail = vec_mem(*dst) - vec_len(*dst);

	if (avail < 3) {
		err = vec_concat(dst, "\0\0", 2);
		if (err) return err;
	}

	return vec_append(dst, &src);
}

int
str_readline(char **dst, FILE *src)
{
	char c = 0;
	int err = 0;
	size_t avail = 0;

	vec_truncat(dst, 0);

	while (fread(&c,1,1,src) && c != '\n') {
		avail = vec_mem(*dst) - vec_len(*dst);

		if (avail < 3) {
			err = vec_concat(dst, "\0\0", 2);
			if (err) return err;
		}

		vec_append(dst, &c);
	}

	if (feof(src)) return -1;
	if (ferror(src)) return errno;
	return 0;
}

void
str_chomp(char **str)
{
	char *nl = 0;
	char *s = *str;

	do if (*s == '\n') nl = s; while (*s++);
	vec_truncat(str, nl - s);
	if (nl) *nl = 0;
}

void
str_free(char *str)
{
	vec_free(str);
}

char *
str_alloc(void)
{
	char *ret;
	vec_ctor(ret);
	return ret;
}
