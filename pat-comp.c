#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <util.h>
#include <vec.h>
#include <pat.ih>
#include <pat.h>

static int comp_cat(struct ins *, struct token const *);
static int comp_lit(struct ins *, struct token const *);
static int comp_nil(struct ins *, struct token const *);

static int (* const tab[])(struct ins *, struct token const *) = {
	[type_nil] = comp_nil,
	[type_lit] = comp_lit,
	[type_cat] = comp_cat,
};

int
comp_cat(struct ins *dst, struct token const *tok)
{
	--tok;
	return tab[tok->id](dst, tok);
}

int
comp_lit(struct ins *dst, struct token const *tok)
{
	*dst++ = (struct ins){
		.op = do_char,
		.arg = {.b = tok--->ch }
	};

	return tab[tok->id](dst, tok);

}

int
comp_nil(struct ins *dst, struct token const *tok)
{
	*dst++ = (struct ins){ .op = do_halt };
	return 0;
}

int
comp_gen(struct ins *dst, struct token *tok)
{
	return tab[tok->id](dst, tok);
}

int
pat_marshal(struct pattern *dest, struct token **tok)
{
	size_t const len = tok[0]->len + 4;
	dest->prog = calloc(len, sizeof *dest->prog);
	if (!dest->prog) return ENOMEM;

	memcpy(dest->prog, (struct ins[]) {
		[0] = { .op = do_jump, .arg = {.f=2}, },
		[1] = { .op = do_clss, .arg = {.i=class_any}, },
		[2] = { .op = do_fork, .arg = {.f=-1}, },
	}, 3 * sizeof *dest->prog);

	comp_gen(dest->prog, *tok);

	*tok = unwind(*tok);

	return 0;
}

