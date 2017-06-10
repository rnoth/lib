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
pat_compile(struct pattern *dst, char const *src)
{
	uintptr_t root = 0;
	struct token *tok;
	int err = 0;

	if (!dst) return EFAULT;
	if (!src) return EFAULT;

	err = pat_scan(&tok, src);
	if (err) goto finally;

	err = pat_parse(&root, tok);
	if (err) goto finally;

	err = pat_marshal(dst, root);
	if (err) goto finally;

	err = vec_ctor(dst->mat);
	if (err) goto finally;

finally:
	nod_dtor(root);
	arr_free(tok);
	return err;
}

void
pat_free(struct pattern *pat)
{
	vec_free(pat->prog);
	vec_free(pat->mat);
}

int
pat_execute(struct pattern *pat, char const *str)
{
	if (!str) return EFAULT;
	return pat_execute_callback(pat, get_char, (struct pos[]){
			{ .n = strlen(str) + 1, .v = str },
	});
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
