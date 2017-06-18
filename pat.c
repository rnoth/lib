#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <vec.h>

#include <pat.h>
#include <pat.ih>

static int get_char(char *, void *);

int
get_char(char *d, void *x)
{
	char **p = x;

	if (!*p) return 0;

	*d = *p[0];

	if (!**p) *p = 0;
	else ++*p;

	return 1;
}

int
pat_compile(struct pattern *dst, char const *src)
{
	struct token *tok = 0;
	int err = 0;

	if (!dst) return EFAULT;
	if (!src) return EFAULT;

	err = pat_parse(&tok, src);
	if (err) goto finally;

	err = pat_marshal(dst, tok);
	if (err) goto finally;

finally:
	tok_free(tok);
	return err;

}

void
pat_free(struct pattern *pat)
{
	free(pat->prog);
}

int
pat_execute(struct pattern *pat, char const *str)
{
	if (!str) return EFAULT;
	return pat_execute_callback(pat, get_char, &str);
}

int
pat_execute_callback(struct pattern *pat, int (*cb)(char *, void *), void *cbx)
{
	if (!pat) return EFAULT;
	if (!cb)  return EFAULT;

	return pat_match(pat, cb, cbx);
}
