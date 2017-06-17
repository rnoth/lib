#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <arr.h>
#include <util.h>
#include <vec.h>

#include <pat.h>
#include <pat.ih>

int
get_char(char *dst, void *x)
{
	char **p = x;

	if (!*p) return 0;

	*dst = *p[0];

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

	err = vec_ctor(dst->mat);
	if (err) goto finally;

finally:
	tok_free(tok);
	return err;

}

void
pat_free(struct pattern *pat)
{
	free(pat->prog);
	vec_free(pat->mat);
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
	struct context ctx[1] = {{ .cb = cb, .cbx = cbx }};
	int err = 0;

	if (!pat) return EFAULT;
	if (!cb)  return EFAULT;

	err = ctx_init(ctx, pat);
	if (err) goto finally;

	err = pat_match(ctx, pat);
	if (err) goto finally;

finally:
	ctx_fini(ctx);
	return err;
}
