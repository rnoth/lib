#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "pat.h"
#include "util.h"
#include "vec.h"

enum pat_char_class {
	class_any   = 0x0,
	class_dot   = 0x1,
	class_alpha = 0x2,
	class_upper = 0x3,
	class_lower = 0x4,
	class_digit = 0x5,
	class_space = 0x6,
};

enum pat_sym {
	sym_literal,
	sym_dot,
	sym_qmark,
	sym_star,
	sym_plus,
	sym_pipe,
	sym_carrot,
	sym_dollar,
	sym_lparen,
	sym_rparen,
	sym_bol,
	sym_eol,
};

enum type {
	type_null,
	type_root,
	type_cat,
	type_sub,
	type_opt,
	type_rep_null,
	type_rep,
	type_alt,
	type_leaf,
};

struct pos {
	char   const *v;
	size_t const  n;
	size_t        f;
};

struct node {
	uintptr_t chld[2];
	enum type type;
};

struct token {
	int          len;
	enum pat_sym type;
	wchar_t      wc;
};

struct state {
	struct pos   str[1];
	struct node  root[1];
	uintptr_t   *stk;
};

struct thread {
	struct patmatch *mat;
	struct ins      *ip;
};

struct context {
	int          (*cb)(char *, void *);
	void          *cbx;
	size_t         pos;
	struct thread *thr;
	struct thread  fin[1];
};

union arg {
	ptrdiff_t f;
	size_t    z;
	wchar_t   w;
	int       i;
};

struct ins {
        int      (*op)(struct context *, struct thread *, wchar_t const);
	union arg arg;
};

static int add_alter(struct state *, struct token *);
static int add_literal(struct state *, struct token *);
static int add_repetition(struct state *, struct token *);
static int add_submatch(struct state *, struct token *);
static int add_subtree(struct state *, struct token *);

static int reduce_generic(struct state *, uintptr_t, uintptr_t);
static int reduce_leaf(struct state *, uintptr_t, uintptr_t);
static int reduce_submatch(struct state *, uintptr_t, uintptr_t);

static int comp_alt(struct ins **, struct node *);
static int comp_cat(struct ins **, struct node *);
static int comp_chld(struct ins **, uintptr_t);
static int comp_root(struct ins **, struct node *);
static int comp_rep(struct ins **, struct node *);
static int comp_sub(struct ins **, struct node *);

static int  ctx_init(struct context *, struct pattern *);
static void ctx_fini(struct context *);

static int get_char(char *, void *);

static int ins_char(struct context *, struct thread *, wchar_t const);
static int ins_clss(struct context *, struct thread *, wchar_t const);
static int ins_fork(struct context *, struct thread *, wchar_t const);
static int ins_halt(struct context *, struct thread *, wchar_t const);
static int ins_jump(struct context *, struct thread *, wchar_t const);
static int ins_mark(struct context *, struct thread *, wchar_t const);
static int ins_save(struct context *, struct thread *, wchar_t const);

static uintptr_t mk_cat(uintptr_t, uintptr_t);

static struct node * nod_ctor(enum type);
static void          nod_dtor(struct node *);
static uint8_t       nod_prec(uintptr_t);
static enum type     nod_type(uintptr_t);

static uintptr_t stk_cat_prec(struct state *, enum type);
static int       stk_pop(uintptr_t *, struct state *);
static int       stk_pop_prec(uintptr_t *, struct state *, enum type);
static int       stk_push(struct state *, uintptr_t);
static int       stk_reduce(struct state *, uintptr_t);

static enum type sym_type(struct token *);

static int  thr_cmp(struct thread *, struct thread *);
static int  thr_init(struct thread *, struct ins *);
static void thr_finish(struct context *, size_t);
static int  thr_fork(struct thread *, struct thread *);
static int  thr_next(struct context *, size_t, wchar_t const);
static void thr_prune(struct context *, size_t);
static int  thr_start(struct context *, wchar_t const);
static void thr_remove(struct context *, size_t);

static void pat_lex(struct token *, struct state *);
static int  pat_marshal(struct pattern *, struct node *);
static int  pat_parse(struct node **, char const *);
static int  pat_parse_tree(struct state *);
static int  pat_match(struct context *, struct pattern *);
static int  pat_exec(struct context *);

static inline
bool        eol(struct pos const *p) { return p->f >= p->n; }

static inline
size_t      rem(struct pos const *p) { return p->n - p->f; }

static inline
char const *str(struct pos const *p) { return p->v + p->f; }

static inline bool nomatch(struct context *ctx) { return !ctx->fin->ip; }

static inline
enum type
type(struct node *n) { return n ? n->type : 0x0; }

static inline
struct node *
to_node(uintptr_t u) { return (void *)(u & ~1); }

static inline
struct ins  *
to_leaf(uintptr_t u) { return (void *)u; }

static inline
bool
is_leaf(uintptr_t u) { return u ? !(u & 1) : false; }

static inline
bool
is_node(uintptr_t u) { return u ? u & 1 : false; }

static inline
bool
is_expr(uintptr_t u)
{
	struct node *nod = to_node(u);
	switch (nod->type) {
	case type_sub:
		if (nod->chld[0]) return true;
		else return false;
	case type_alt:
		if (nod->chld[0] && nod->chld[1]) {
			return true;
		} else return false;
	default:
		return true;
	}
}

static inline
bool
is_complete(struct node *n)
{
	switch (n->type) {
	case type_sub:
		return !!n->chld[1];
	case type_alt:
		return n->chld[0] && n->chld[1];
	}

	abort();
}

static inline
uintptr_t
tag_leaf(struct ins *i) { return (uintptr_t)i; }

static inline
uintptr_t
tag_node(struct node *n) { return n ? (uintptr_t)n + 1 : 0x0; }

static int (* const pat_add[])(struct state *, struct token *) = {
	[sym_pipe]    = add_alter,
	[sym_literal] = add_literal,
	[sym_qmark]   = add_repetition,
	[sym_star]    = add_repetition,
	[sym_plus]    = add_repetition,
	[sym_lparen]  = add_submatch,
	[sym_rparen]  = add_subtree,
	[sym_bol]     = add_submatch,
	[sym_eol]     = add_subtree,
};

static int (* const pat_reduce[])(struct state *, uintptr_t, uintptr_t) = {
	[type_alt]      = reduce_generic,
	[type_opt]      = reduce_generic,
	[type_rep_null] = reduce_generic,
	[type_rep]      = reduce_generic,
	[type_leaf]     = reduce_leaf,
	[type_sub]      = reduce_submatch,
};

static int (* const pat_comp[])(struct ins **, struct node *) = {
	[type_root]     = comp_root,
	[type_alt]      = comp_alt,
	[type_cat]      = comp_cat,
	[type_opt]      = comp_rep,
	[type_rep_null] = comp_rep,
	[type_rep]      = comp_rep,
	[type_sub]      = comp_sub,
};

static uint8_t const pat_prec[] = {
	[type_cat]      =  1,
	[type_alt]      =  3,
	[type_opt]      =  2,
	[type_rep_null] =  2,
	[type_rep]      =  2,
	[type_root]     = -1,
	[type_sub]      = -1,
};

int
add_alter(struct state *st, struct token *tok)
{
	struct node *alt;
	uintptr_t cat;

	alt = nod_ctor(type_alt);
	if (!alt) return ENOMEM;

	cat = stk_cat_prec(st, type_alt);
	if (!cat) { free(alt); return ENOMEM; }

	alt->chld[0] = cat;

	return stk_push(st, tag_node(alt));
}

int
add_literal(struct state *st, struct token *tok)
{
	struct ins *p;
	
	p = calloc(1, sizeof *p);
	if (!p) return ENOMEM;

	*p = (struct ins) {
		.op = ins_char,
		.arg = { .w = tok->wc },
	};

	return stk_push(st, tag_leaf(p));
}

int
add_repetition(struct state *st, struct token *tok)
{
	struct node *rep;
	enum type ty = sym_type(tok);
	uintptr_t chld;
	int err;

	rep = nod_ctor(ty);
	if (!rep) return ENOMEM;

	if (stk_pop_prec(&chld, st, ty)) {
		err = PAT_ERR_BADREP;
		goto fail;
	}

	rep->chld[0] = chld;

	err = stk_push(st, tag_node(rep));
	if (err) goto fail;

	return 0;
fail:
	free(rep);
	return err;
}

int
add_submatch(struct state *st, struct token *tok)
{
	struct node *new;
	int err;

	new = nod_ctor(type_sub);
	if (!new) return ENOMEM;

	err = stk_push(st, tag_node(new));
	if (err) { free(new); return err; }

	return 0;
}

int
add_subtree(struct state *st, struct token *tok)
{
	uintptr_t chld;

	if (stk_pop(&chld, st)) return PAT_ERR_BADPAREN;
	return stk_reduce(st, chld);
}

int
reduce_generic(struct state *st, uintptr_t par, uintptr_t chld)
{
	to_node(par)->chld[1] = chld;
	return stk_reduce(st, par);
}

int
reduce_leaf(struct state *st, uintptr_t par, uintptr_t chld)
{
	uintptr_t res = mk_cat(par, chld);

	if (!res) return ENOMEM;
	else return stk_reduce(st, res);
}

int
reduce_submatch(struct state *st, uintptr_t par, uintptr_t chld)
{
	struct node *sub = to_node(par);

	if (sub->chld[0]) {
		sub->chld[1] = chld;
		return stk_reduce(st, par);
	}
	sub->chld[0] = chld;

	stk_push(st, par);

	return 0;
}

int
comp_alt(struct ins **dest, struct node *alt)
{
	size_t fork;
	size_t jump;
	int err;

	fork = vec_len(*dest);
	err = vec_append(dest, (struct ins[]) {
		{ .op = ins_fork }
	});
	if (err) return err;

	err = comp_chld(dest, alt->chld[0]);
	if (err) return err;

	jump = vec_len(*dest);
	err = vec_append(dest, (struct ins[]) {
		{ .op = ins_jump }
	});
	if (err) return err;

	dest[0][fork].arg.f = vec_len(*dest) - fork;

	err = comp_chld(dest, alt->chld[1]);
	if (err) return err;

	dest[0][jump].arg.f = vec_len(*dest) - jump;

	return 0;
}

int
comp_cat(struct ins **dest, struct node *nod)
{
	int err = 0;

	err = comp_chld(dest, nod->chld[0]);
	if (err) return err;

	return comp_chld(dest, nod->chld[1]);
}

int
comp_chld(struct ins **dest, uintptr_t n)
{
	if (!n) return 0;
	if (is_leaf(n)) return vec_append(dest, to_leaf(n));
	struct node *nod = to_node(n);
	return pat_comp[nod->type](dest, nod);
}

int
comp_root(struct ins **dest, struct node *root)
{
	int err = 0;

	err = vec_concat_arr(dest, ((struct ins[]) {
		[0] = { .op = ins_jump, .arg = {.f=2}, },
		[1] = { .op = ins_clss, .arg = {.i=class_any}, },
		[2] = { .op = ins_fork, .arg = {.f=-1}, },
	}));
	if (err) goto finally;

	err = comp_chld(dest, root->chld[0]);
	if (err) goto finally;

	err = vec_append(dest, ((struct ins[]) {
		{ .op = ins_halt, .arg = {0}, },
	}));
	if (err) goto finally;

finally:
	if (err) vec_truncat(dest, 0);
	return err;
}

int
comp_rep(struct ins **dest, struct node *rep)
{
	size_t beg;
	size_t off;
	int err;

	off = vec_len(*dest);

	if (type(rep) != type_rep) {
		err = vec_append(dest, (struct ins[]) {{ .op = ins_fork }});
		if (err) return err;
	}

	beg = vec_len(*dest);

	err = comp_chld(dest, rep->chld[0]);
	if (err) return err;

	if (type(rep) != type_opt) {
		err = vec_append(dest, ((struct ins[]) {{
			.op = ins_fork,
			.arg = { .f = beg - vec_len(*dest) },
		}}));
		if (err) return err;
	}

	if (type(rep) != type_rep) {
		(*dest)[off].arg.f = vec_len(*dest) - off;
	}

	return comp_chld(dest, rep->chld[1]);
}

int
comp_sub(struct ins **dest, struct node *root)
{
	int err;

	err = vec_append(dest, ((struct ins[]) {
		{ .op = ins_mark, .arg = {0}, },
	}));
	if (err) goto finally;

	err = comp_chld(dest, root->chld[0]);
	if (err) goto finally;

	err = vec_append(dest, ((struct ins[]) {
		{ .op = ins_save, .arg = {0}, },
	}));
	if (err) goto finally;

	err = comp_chld(dest, root->chld[1]);
	if (err) goto finally;

finally:
	if (err) vec_truncat(dest, 0);
	return err;
}


int
ctx_init(struct context *ctx, struct pattern *pat)
{
	struct thread th[1] = {{0}};
	int err = 0;

	err = thr_init(th, pat->prog);
	if (err) return err;

	err = vec_ctor(ctx->thr);
	if (err) goto fail;

	err = vec_append(&ctx->thr, th);
	if (err) goto fail;

	return 0;

fail:
	vec_free(th->mat);
	vec_free(ctx->thr);
	return err;
}

void
ctx_fini(struct context *ctx)
{
	struct thread *th;

	vec_foreach(th, ctx->thr) vec_free(th->mat);
	vec_free(ctx->thr);
	vec_free(ctx->fin->mat);
}

int
get_char(char *dst, void *x)
{
	union {
		void *p;
		struct pos *v;
	} str = { .p = x };
	struct pos *p = str.v;
	int len = 0;

	if (p->f > p->n) return 0;
	len = mblen(p->v + p->f, p->n - p->f);
	if (len <= 0) len = 1;

	memcpy(dst, p->v + p->f, len);

	p->f += len;

	return len;
}

int
ins_char(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t ind = th - ctx->thr;

	if (th->ip->arg.w != wc) thr_remove(ctx, th - ctx->thr), --ind;
	else ++th->ip;

	return thr_next(ctx, ind + 1, wc);
}

int
ins_clss(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t ind = th - ctx->thr;
	bool res;

	switch (th->ip->arg.i) {
	case class_any: res = true; break;
	case class_dot: res = wc != L'\n' && wc != L'\0'; break;
	case class_alpha: res = iswalpha(wc); break;
	case class_upper: res = iswupper(wc); break;
	case class_lower: res = iswlower(wc); break;
	case class_space: res = iswspace(wc); break;
	case class_digit: res = iswdigit(wc); break;
	default:
		return -1; // XXX
	}

	if (!res) thr_remove(ctx, ind), --ind;
	else ++th->ip;

	return thr_next(ctx, ind + 1, wc);
}

int
ins_fork(struct context *ctx, struct thread *th, wchar_t const wc)
{
	struct thread new[1] = {{0}};
	size_t ind = th - ctx->thr;
	int err = 0;

	err = thr_fork(new, th);
	if (err) goto fail;

	new->ip += th->ip->arg.f;

	err = vec_insert(&ctx->thr, new, ind + 1);
	if (err) goto fail;

	th = ctx->thr + ind;

	++th->ip;
	return th->ip->op(ctx, th, wc);

fail:
	vec_free(new->mat);
	return err;
}

int
ins_halt(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t ind = th - ctx->thr;

	if (thr_cmp(ctx->fin, th) > 0) {
		thr_remove(ctx, ind);
		return thr_next(ctx, ind, wc);
	}

	thr_finish(ctx, ind);
	return thr_next(ctx, ind, wc);
}

int
ins_jump(struct context *ctx, struct thread *th, wchar_t const wc)
{
	th->ip += th->ip->arg.f;
	return th->ip->op(ctx, th, wc);
}

int
ins_mark(struct context *ctx, struct thread *th, wchar_t const wc)
{
	int err = 0;
	struct patmatch mat = {0};

	mat.off = ctx->pos;
	mat.ext = -1;

	err = vec_append(&th->mat, &mat);
	if (err) return err;

	++th->ip;
	return th->ip->op(ctx, th, wc);
}

int
ins_save(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t off = vec_len(th->mat);

	while (off --> 0) if (th->mat[off].ext == (size_t)-1) break;

	th->mat[off].ext = ctx->pos - th->mat[off].off;

	th->ip++;
	return th->ip->op(ctx, th, wc);
}

uintptr_t
mk_cat(uintptr_t left, uintptr_t right)
{
	struct node *ret;

	ret = nod_ctor(type_cat);
	if (ret) {
		ret->chld[0] = left;
		ret->chld[1] = right;
	}

	return tag_node(ret);
}
 
struct node *
nod_ctor(enum type type)
{
	struct node *ret = calloc(1, sizeof *ret);

	if (!ret) return 0x0;

	ret->type = type;

	return ret;
}

void
nod_dtor(struct node *nod)
{
	uintptr_t u;

	if (!nod) return;

	arr_foreach(u, nod->chld) {
		if (is_leaf(u)) free(to_leaf(u));
		else nod_dtor(to_node(u));
	}

	free(nod);
}

uint8_t
nod_prec(uintptr_t u)
{
	if (is_expr(u)) return 0;
	else return pat_prec[nod_type(u)];
}

enum type
nod_type(uintptr_t u)
{
	if (is_leaf(u)) return type_leaf;
	else return to_node(u)->type;
}

uintptr_t
stk_cat_prec(struct state *st, enum type type)
{
	uintptr_t cat = 0;
	uintptr_t tmp = 0;

	while (!stk_pop_prec(&tmp, st, type_alt)) {
		if (cat) cat = mk_cat(tmp, cat);
		else cat = tmp;
		if (!cat) goto nomem;
	}

	return cat;
nomem:
	nod_dtor(to_node(cat));
	return 0x0;
}

int
stk_pop(uintptr_t *u, struct state *st)
{
	if (!vec_len(st->stk)) return -1;

	vec_pop(u, &st->stk);

	return 0;
}

int
stk_pop_prec(uintptr_t *dst, struct state *st, enum type type)
{
	size_t len;
	uintptr_t top;

	len = vec_len(st->stk);
	if (len == 0) return -1;

	top = st->stk[len - 1];

	if (is_leaf(top));
	else if (nod_prec(top) >= pat_prec[type]) return -1;

	vec_truncat(&st->stk, len - 1);

	*dst = top;

	return 0;
}

int
stk_push(struct state *st, uintptr_t u)
{
	return vec_append(&st->stk, (uintptr_t[]){u});
}

int
stk_reduce(struct state *st, uintptr_t chld)
{
	uintptr_t par;

	if (stk_pop(&par, st)) return 0;
	else return pat_reduce[nod_type(par)](st, par, chld);
}

int
thr_cmp(struct thread *lt, struct thread *rt)
{
	size_t min = 0;
	ptrdiff_t cmp = 0;
	size_t l = 0;
	size_t r = 0;

	if (!lt->mat && !rt->mat) return 0;
	if (!lt->mat) return -1;
	if (!rt->mat) return  1;

	min = umin(vec_len(lt->mat), vec_len(rt->mat));

	iterate(i, min) {
		l = lt->mat[i].off;
		r = rt->mat[i].off;
		cmp = ucmp(l, r);
		if (cmp) return -cmp;
		l = lt->mat[i].ext;
		r = rt->mat[i].ext;
		cmp = ucmp(l, r);
		if (cmp) return cmp;
	}

	return memcmp((size_t[]){vec_len(lt->mat)},
	              (size_t[]){vec_len(rt->mat)},
		      sizeof vec_len(0)); // ?
}

enum type
sym_type(struct token *tok)
{
	enum type const tab[] = {
		[sym_qmark] = type_opt,
		[sym_plus]  = type_rep,
		[sym_star]  = type_rep_null,
	};

	return tab[tok->type];
}

int
thr_init(struct thread *th, struct ins *prog)
{
	th->ip = prog;

	th->mat = vec_new(struct thread);
	if (!th->mat) return ENOMEM;;

	return 0;
}

void
thr_finish(struct context *ctx, size_t ind)
{
	if (ctx->fin->mat) vec_free(ctx->fin->mat);
	memcpy(ctx->fin, ctx->thr + ind, sizeof *ctx->fin);

	vec_delete(&ctx->thr, ind);
}

int
thr_fork(struct thread *dst, struct thread *src)
{
	dst->ip = src->ip;
	dst->mat = vec_clone(src->mat);

	return dst->mat ? 0 : ENOMEM;
}

int
thr_next(struct context *ctx, size_t ind, wchar_t wc)
{
	if (!vec_len(ctx->thr)) return 0;
	if (ind >= vec_len(ctx->thr)) return 0;

	if (ctx->fin->mat) thr_prune(ctx, ind);

	if (ind >= vec_len(ctx->thr)) return 0;
	return ctx->thr[ind].ip->op(ctx, ctx->thr + ind, wc);
}

void
thr_prune(struct context *ctx, size_t ind)
{
	size_t i;

	for (i = ind; i < vec_len(ctx->thr);) {
		if (thr_cmp(ctx->fin, ctx->thr + i) > 0) thr_remove(ctx, i);
		else return;
	}
}

int
thr_start(struct context *ctx, wchar_t wc)
{
	return thr_next(ctx, 0, wc);
}

void
thr_remove(struct context *ctx, size_t ind)
{
	vec_free(ctx->thr[ind].mat);
	vec_delete(&ctx->thr, ind);
}

int
pat_marshal(struct pattern *dest, struct node *root)
{
	return pat_comp[root->type](&dest->prog, root);
}

int
pat_match(struct context *ctx, struct pattern *pat)
{
	int err = pat_exec(ctx);
	if (err) return err;

	return vec_copy(&pat->mat, ctx->fin->mat);
}

int
pat_exec(struct context *ctx)
{
	char c[8];
	int err;
	int len;
	wchar_t wc = 0;
	wchar_t *wcs = 0;

	err = vec_ctor(wcs);
	if (err) return err;

	while (ctx->cb(c, ctx->cbx)) {

		len = mbtowc(&wc, c, 8);
		if (len <= 0) wc = *c; // XXX

		err = vec_concat(&wcs, &wc, len);
		if (err) break;

		err = thr_start(ctx, wc);
		if (err) break;

		ctx->pos += len;
	}
	
	vec_free(wcs);

	if (err > 0) return err;
	if (nomatch(ctx)) return -1;

	return 0;
}

void
pat_lex(struct token *tok, struct state *st)
{
	struct pos *pos = st->str;

	tok->len = 1;
	switch (*str(pos)) {
	case '.':

		tok->type = sym_dot;
		break;

	case '?':
		tok->type = sym_qmark;
		break;

	case '*':
		tok->type = sym_star;
		break;

	case '+':
		tok->type = sym_plus;
		break;

	case '(':
		tok->type = sym_lparen;
		break;

	case ')':
		tok->type = sym_rparen;
		break;

	case '|':
		tok->type = sym_pipe;
		break;

	case '^':
		tok->type = sym_carrot;
		break;

	case '$':
		tok->type = sym_dollar;
		break;

	case '\\':
		goto esc;

	default:
		tok->type = sym_literal;
		tok->len = mbtowc(&tok->wc, str(pos), rem(pos));

		if (tok->len == -1) tok->wc = pos->v[0], tok->len = 1;
		else if (tok->len == 0) tok->len = 1;
	}
 
 	pos->f += tok->len;
	return;

esc:  // XXX
	switch (pos->v[pos->f + 1]) {
	default:
		tok->type = sym_literal;
		int tmp = mbtowc(&tok->wc, str(pos) + 1, rem(pos) - 1);

		if (tmp == -1) tmp = 1, tok->wc = pos->v[0];
		else if (tmp == 0) tmp = 1;

		tok->len += tmp;
	}

 	pos->f += tok->len;
	return;
}

int
pat_parse(struct node **rootp, char const *src)
{
	struct state st[1] = {{
		.str = {{ .v = src, .n = strlen(src), }}
	}};
	int err = 0;

	*rootp = nod_ctor(type_root);
	if (!*rootp) return ENOMEM;

	err = vec_ctor(st->stk);
	if (err) goto finally;

	err = pat_parse_tree(st);
	if (err) goto finally;

	rootp[0]->chld[0] = st->stk[0];

finally:
	vec_free(st->stk);

	return err;
}

int
pat_parse_tree(struct state *st)
{
	struct token tok[1] = {{0}};
	int err = 0;

	err = pat_add[sym_bol](st, tok);
	if (err) return err;

	while (!eol(st->str)) {
		pat_lex(tok, st);

		err = pat_add[tok->type](st, tok);
		if (err) return err;
	}

	*tok = (struct token) {0};

	err = pat_add[sym_eol](st, tok);
	if (err) return err;

	st->root->chld[0] = st->stk[0];
	return 0;
}

int
pat_compile(struct pattern *dest, char const *src)
{
	int err = 0;
	struct node *root = 0x0;

	if (!dest) return EFAULT;
	if (!src) return EFAULT;

	err = vec_ctor(dest->prog);
	if (err) goto finally;

	err = vec_ctor(dest->mat);
	if (err) goto finally;

	err = pat_parse(&root, src);
	if (err) goto finally;

	err = pat_marshal(dest, root);
	if (err) goto finally;

finally:
	nod_dtor(root);
	if (err) vec_free(dest->prog);
	if (err) vec_free(dest->mat);
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
