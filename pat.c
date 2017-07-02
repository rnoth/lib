#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <vec.h>

#include <pat.h>
#include <pat.ih>

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
	if (!pat) return EFAULT;

	struct context ctx[1] = {{
		.str = str,
		.len = strlen(str),
	}};

	return pat_match(pat, ctx);
}
