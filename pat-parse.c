#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <vec.h>
#include <util.h>
#include <pat.h>
#include <pat.ih>

static int (* const tab_shift[])(struct state *) = {
	['\\'] = shift_escape,
	['*']  = shift_token,
	['?']  = shift_token,
	['+']  = shift_token,
	['(']  = shift_token,
	[')']  = shift_close,
	[255]  = 0,
};

static int (* const tab_shunt[])(struct state *) = {
	[255] = 0,
};

static int (* const tab_grow[])(struct state *) = {
	['\\'] = grow_escape,
	['_']  = grow_cat,
	['(']  = grow_open,
	[')']  = grow_close,
	[255]  = 0,
};

int
grow_cat(struct state *st)
{
	uintptr_t cat;
	uintptr_t lef;
	uintptr_t rit;

	vec_get(&rit, st->trv);
	vec_get(&lef, st->trv);

	cat = mk_cat(lef, rit);
	if (!cat) {
		vec_put(st->trv, &lef);
		vec_put(st->trv, &rit);
		return ENOMEM;
	}

	vec_put(&st->trv, (uintptr_t[]){tag_node(cat)});

	++st->ir;
	return pat_grow(st);
}

int
grow_char(struct state *st)
{
	uintptr_t prev;
	uint8_t *res;

	prev = st->trv[vec_len(st->trv)-1];

	if (!is_leaf(prev)) {
		if (vec_len(st->aux)) vec_put(st->aux, (uint8_t[]){0});
		res = st->aux + vec_len(st->aux);
		vec_put(st->aux, st->ir);
		vec_put(st->trv, (uintptr_t[]){tag_leaf(res)});
	} else vec_put(st->aux, st->ir);

	++st->ir;

	return pat_grow(st);
}

int
grow_close(struct state *st)
{
	uintptr_t lef;
	uintptr_t rit;
	uintptr_t res;

again:
	vec_get(&rit, st->trv);
	if (is_open(rit)) {
		vec_put(st->trv, &rit);
		return pat_grow(st);
	}

	vec_get(&lef, st->trv);
	res = nod_attach(lef, rit);

	vec_put(st->trv, &res);

	if (nod_type(res) != type_sub) goto again;

	++st->ir;
	return pat_grow(st);
}

int
grow_escape(struct state *st)
{
	++st->ir;
	return grow_char(st);
}

int
grow_open(struct state *st)
{
	struct node *nod;

	nod = calloc(1, sizeof *nod);
	if (!nod) return ENOMEM;

	nod->type = type_sub;
	vec_append(&st->trv, (uintptr_t[]){tag_node(nod)});

	++st->ir;
	return pat_grow(st);
}

int
grow_rep(struct state *st)
{
	return -1;
}

int
shift_close(struct state *st)
{
	return shift_token(st);
}

int
shift_escape(struct state *st)
{
	step(st->src);
	vec_put(st->stk, (char[]){'\\'});
	vec_put(st->stk, str(st->src));
	step(st->src);
	return pat_shunt(st);
}

int
shift_liter(struct state *st)
{
	uint8_t ch; 

	memcpy(&ch, str(st->src), 1);
	if (tab_shift[ch]) {
		vec_put(st->stk, (char[]){'\\'});
	}

	vec_put(st->stk, str(st->src));

	step(st->src);
	return pat_shunt(st);
}

int
shift_token(struct state *st)
{
	vec_put(st->aux, str(st->src));
	step(st->src);

	return pat_shunt(st);
}

void
st_fini(struct state *st)
{
	vec_free(st->stk);
	vec_free(st->aux);
	vec_free(st->trv);
}

int
st_init(struct state *st, char const *src)
{
	memcpy(st->src, (struct pos[]){{
		.v = src,
		.n = strlen(src)
	}}, sizeof *st->src);
	st->stk = vec_alloc(uint8_t, st->src->n * 2 + 2);
	st->aux = vec_alloc(uint8_t, st->src->n * 2);
	st->trv = vec_alloc(uintptr_t, st->src->n);

	if (!st->aux || !st->stk || !st->trv) {
		vec_free(st->stk);
		vec_free(st->aux);
		vec_free(st->trv);

		return ENOMEM;
	}

	return 0;
}

int
pat_grow(struct state *st)
{
	uint8_t ch;
	int (*fn)();
	size_t off = st->ir - st->stk;

	if (off >= vec_len(st->stk)) return 0;

	memcpy(&ch, st->ir, 1);

	if (!ch) return 0;

	fn = tab_grow[ch];
	if (fn) return fn(st);
	else return grow_char(st);
}

int
pat_flush(struct state *st)
{
	st->ir = st->stk;
	vec_zero(st->aux);
	return 0;
}

int
pat_process(struct state *st)
{
	int err;

	vec_put(st->stk, (char[]){'('});
	err = pat_shunt(st);
	if (err) return err;
	vec_put(st->stk, (char[]){')'});

	return 0;
}

int
pat_shunt(struct state *st)
{
	int (*fn)();
	uint8_t ch;

	if (eol(st->src)) return pat_flush(st);

	memcpy(&ch, str(st->src), 1);

	fn = tab_shift[ch];
	if (fn) return fn(st);
	else return shift_liter(st);
}

int
pat_parse(uintptr_t *dst, struct state *st)
{
	int err;

	err = pat_process(st);
	if (err) return err;

	err = pat_grow(st);
	if (err) return err;

	*dst = st->trv[0];

	return 0;
}
