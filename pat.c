#include <assert.h>
#include <errno.h>
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
};

enum type {
	type_null,
	type_root,
	type_opt,
	type_rep_null,
	type_rep,
	type_alt,
};

struct context {
	int          (*cb)(char *, void *);
	void          *cbx;
	size_t         pos;
	struct thread *thr;
	struct thread *fin;
};

struct node {
	uintptr_t *chld;
	struct node *up;
	enum type type;
};

struct pos {
	char   const*v;
	size_t const n;
	size_t       f;
};

struct thread {
	struct patmatch *mat;
	struct ins 	*ins;
	size_t		 pos;
};

struct token {
	size_t		 len;
	enum pat_sym	 type;
	wchar_t		 wc;
};

struct state {
	struct pos   str[1];
	struct token tok[1];
	struct node *cur;
	struct ins **buf;
	size_t       dep;
};

struct ins {
        int      (*op)(struct context *, struct thread *, wchar_t const);
	uint64_t  arg;
};

static inline struct ins *ip(struct thread *);

static int add_literal(struct state *, struct pattern *);
static int add_repetition(struct state *, struct pattern *);
static int add_submatch(struct state *, struct pattern *);

static int comp_opt(struct ins **, struct node *);
static int comp_recurse(struct ins **, uintptr_t *);
static int comp_rep(struct ins **, struct node *);
static int comp_rep_null(struct ins **, struct node *);
static int comp_root(struct ins **, struct node *);

static int  ctx_init(struct context *);
static void ctx_fini(struct context *);

static int get_char(char *, void *);

static int ins_char(struct context *, struct thread *, wchar_t const);
static int ins_clss(struct context *, struct thread *, wchar_t const);
static int ins_fork(struct context *, struct thread *, wchar_t const);
static int ins_halt(struct context *, struct thread *, wchar_t const);
static int ins_jump(struct context *, struct thread *, wchar_t const);
static int ins_mark(struct context *, struct thread *, wchar_t const);
static int ins_save(struct context *, struct thread *, wchar_t const);

static struct node *nod_ctor(struct node *);
static void         nod_free(struct node *);

static int  st_flush(struct state *);
static void st_surface(struct state *);

static int  thr_next(struct context *, size_t, wchar_t const);
static int  thr_finish(struct context *, size_t);
static int  thr_fork(struct thread *, struct thread *);
static int  thr_start(struct context *, wchar_t const);
static void thr_remove(struct context *, size_t);

static size_t pat_lex(struct state *);
static int    pat_marshal(struct pattern *, struct node *);
static int    pat_parse(struct node **, char const *, struct pattern *);
static int    pat_do_match(struct pattern *, struct context *);
static int    pat_exec(struct context *);

static inline struct ins * ip(struct thread *th) { return th->ins + th->pos; }

static inline bool        eol(struct pos const *p) { return p->f >= p->n; }
static inline size_t      rem(struct pos const *p) { return p->n - p->f; }
static inline char const *str(struct pos const *p) { return p->v + p->f; }

static inline struct node *to_node(uintptr_t u) { return (void *)(u & ~1); }
static inline struct ins  *to_leaf(uintptr_t u) { return (void *)u; }
static inline bool is_leaf(uintptr_t u) { return u ? !(u & 1) : false; }
static inline uintptr_t tag_leaf(struct ins *i) { return (uintptr_t)i; }
static inline uintptr_t tag_node(struct node *n) { return (uintptr_t)n + 1; }

static int (* const pat_add[])(struct state *, struct pattern *) = {
	[sym_literal] = add_literal,
	[sym_qmark]   = add_repetition,
	[sym_star]    = add_repetition,
	[sym_plus]    = add_repetition,
	[sym_lparen]  = add_submatch,
	[sym_rparen]  = 0x0,
};

static int (* const pat_comp[])(struct ins **, struct node *) = {
	[type_root]     = comp_root,
	[type_opt]      = comp_opt,
	[type_rep_null] = comp_rep_null,
	[type_rep]      = comp_rep,
};

int
add_literal(struct state *st, struct pattern *pat)
{
	struct ins *p = calloc(1, sizeof *p);
	if (!p) return ENOMEM;

	memcpy(p, (struct ins[]){{.arg=st->tok->wc, .op=ins_char}}, sizeof *p);

	return vec_append(&st->buf, &p);
}

int
add_repetition(struct state *st, struct pattern *pat)
{
	enum type const tab[] = {
		[sym_qmark] = type_opt,
		[sym_plus]  = type_rep,
		[sym_star]  = type_rep_null,
	};
	struct node *nod = 0x0;
	struct ins *ins = 0x0;
	int err = 0;

	nod = nod_ctor(st->cur);
	if (!nod) goto nomem;

	nod->type = tab[st->tok->type];

	vec_pop(&ins, &st->buf);

	err = vec_append(&nod->chld, (uintptr_t[]){ tag_leaf(ins) });
	if (err) goto fail;

	err = st_flush(st);
	if (err) goto fail;

	vec_truncat(&st->buf, 0);

	err = vec_append(&st->cur->chld, (uintptr_t[]){ tag_node(nod) });
	if (err) goto fail;

	return 0;

nomem:
	err = ENOMEM;
fail:
	nod_free(nod);
	free(ins);
	return err;
}

int
add_submatch(struct state *st, struct pattern *pat)
{
	int err = 0;
	while (st->str->n > st->str->f) {
		st->str->f += pat_lex(st);
		err = pat_add[st->tok->type](st, pat);
		if (err) break;
	}

	return err;
}

int
comp_opt(struct ins **dest, struct node *nod)
{
	size_t off = 0;
	int err = 0;

	off = vec_len(*dest);

	err = vec_append(dest, (struct ins[]) {{ .op = ins_fork }});
	if (err) return err;

	err = comp_recurse(dest, nod->chld);
	if (err) return err;

	(*dest)[off].arg = vec_len(*dest);

	return 0;
}

int
comp_recurse(struct ins **dest, uintptr_t *chld)
{
	int err = 0;
	uintptr_t *u;

	vec_foreach (u, chld) {
		if (is_leaf(*u)) err = vec_append(dest, to_leaf(*u));
		else err = pat_comp[to_node(*u)->type](dest, to_node(*u));

		if (err) return err;
	}

	return 0;
}

int
comp_rep(struct ins **dest, struct node *nod)
{
	int err = 0;
	size_t beg = 0;

	beg = vec_len(*dest);

	err = comp_recurse(dest, nod->chld);
	if (err) return err;

	return vec_append(dest, ((struct ins[]) {{
		.arg = beg,
		.op = ins_fork,
	}}));
}

int
comp_rep_null(struct ins **dest, struct node *nod)
{
	int err = 0;
	size_t beg = 0;
	size_t off = 0;

	off = vec_len(*dest);

	err = vec_append(dest, (struct ins[]) {{ .op = ins_fork }});
	if (err) return err;

	beg = vec_len(*dest);

	err = comp_recurse(dest, nod->chld);
	if (err) return err;

	err = vec_append(dest, ((struct ins[]) {{
		.arg = beg,
		.op = ins_fork,
	}}));
	if (err) return err;

	(*dest)[off].arg = vec_len(*dest);

	return 0;
}

int
comp_root(struct ins **dest, struct node *root)
{
	int err = 0;

	err = vec_concat_arr(dest, ((struct ins[]) {
		[0] = { .op = ins_jump, .arg = 2, },
		[1] = { .op = ins_clss, .arg = class_any, },
		[2] = { .op = ins_fork, .arg = 1, },
		[3] = { .op = ins_mark, .arg = 0, },
	}));
	if (err) goto finally;

	err = comp_recurse(dest, root->chld);
	if (err) goto finally;

	err = vec_concat_arr(dest, ((struct ins[]) {
		[0] = { .op = ins_save, .arg = 0, },
		[1] = { .op = ins_halt, .arg = 0, },
	}));
	if (err) goto finally;

finally:
	if (err) vec_truncat(dest, 0);
	return err;
}

int
ctx_init(struct context *ctx)
{
	int err = 0;

	err = vec_ctor(ctx->thr);
	if (err) goto nomem;

	err = vec_ctor(ctx->fin);
	if (err) goto nomem;

	return 0;
nomem:
	vec_free(ctx->thr);
	vec_free(ctx->fin);
	return err;

}

void
ctx_fini(struct context *ctx)
{
	struct thread *tmp;

	vec_foreach(tmp, ctx->thr) vec_free(tmp->mat);
	vec_foreach(tmp, ctx->fin) vec_free(tmp->mat);
	vec_free(ctx->thr);
	vec_free(ctx->fin);
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

	if ((wchar_t)ip(th)->arg != wc) thr_remove(ctx, th - ctx->thr), --ind;
	else ++th->pos;

	return thr_next(ctx, ind + 1, wc);
}

int
ins_clss(struct context *ctx, struct thread *th, wchar_t const wc)
{
	size_t ind = th - ctx->thr;
	bool res;

	switch (ip(th)->arg) {
	case class_any: res = true; break;
	case class_dot: res = wc != L'\n' && wc != L'\0'; break;
	case class_alpha: res = iswalpha(wc); break;
	case class_upper: res = iswupper(wc); break;
	case class_lower: res = iswlower(wc); break;
	case class_space: res = iswspace(wc); break;
	case class_digit: res = iswdigit(wc); break;
	default:
		assert(!"invalid argument to character class instruction");
	}

	if (!res) thr_remove(ctx, ind), --ind;
	else ++th->pos;

	return thr_next(ctx, ind + 1, wc);
}

int
ins_fork(struct context *ctx, struct thread *th, wchar_t const wc)
{
	int err = 0;
	size_t ind = th - ctx->thr;
	struct thread new = {0};

	err = thr_fork(&new, th);
	if (err) goto fail;

	new.pos = ip(th)->arg;

	err = vec_insert(&ctx->thr, &new, th - ctx->thr + 1);
	if (err) goto fail;

	th = ctx->thr + ind;

	++th->pos;
	return ip(th)->op(ctx, th, wc);

fail:
	vec_free(new.mat);
	return err;
}

int
ins_halt(struct context *ctx, struct thread *th, wchar_t const wc)
{
	int err = 0;
	size_t ind = th - ctx->thr;

	err = thr_finish(ctx, ind);
	if (err) return err;

	return thr_next(ctx, ind, wc);
}

int
ins_jump(struct context *ctx, struct thread *th, wchar_t const wc)
{
	th->pos = ip(th)->arg;
	return ip(th)->op(ctx, th, wc);
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

	++th->pos;
	return ip(th)->op(ctx, th, wc);
}

int
ins_save(struct context *ctx, struct thread *th, wchar_t const wc)
{
	struct ins *ins = th->ins + th->pos;

	th->mat[ins->arg].ext = ctx->pos - th->mat[ins->arg].off;
	++th->pos;

	return ip(th)->op(ctx, th, wc);
}

struct node *
nod_ctor(struct node *par)
{
	struct node *ret;

	ret = calloc(1, sizeof *ret);
	if (!ret) return 0x0;

	if (vec_ctor(ret->chld)) {
		free(ret);
		return 0x0;
	}

	ret->up = par;

	return ret;
}

void
nod_free(struct node *nod)
{
	uintptr_t *u;
	if (!nod) return;
	vec_foreach(u, nod->chld) {
		if (is_leaf(*u)) free(to_leaf(*u));
		else nod_free(to_node(*u));
	}

	vec_free(nod->chld);

	free(nod);
}


int
st_flush(struct state *st)
{
	struct ins **ins = 0x0;
	int err = 0;

	vec_foreach (ins, st->buf) {
		err = vec_append(&st->cur->chld, (uintptr_t[]){ tag_leaf(*ins) });
		if (err) return err;
	}

	return 0;
}

void
st_surface(struct state *st)
{
	while (st->cur->up) st->cur = st->cur->up;
}

int
thr_finish(struct context *ctx, size_t ind)
{
	int err = 0;

	err = vec_append(&ctx->fin, ctx->thr + ind);
	if (err) return err;
	vec_delete(&ctx->thr, ind);

	return 0;
}

int
thr_fork(struct thread *dst, struct thread *src)
{
	dst->ins = src->ins;
	dst->pos = src->pos;
	dst->mat = vec_clone(src->mat);

	return dst->mat ? 0 : ENOMEM;
}

int
thr_next(struct context *ctx, size_t ind, wchar_t wc)
{
	if (!vec_len(ctx->thr)) return -1;
	if (ind >= vec_len(ctx->thr)) return 0;
	else return ip(ctx->thr + ind)->op(ctx, ctx->thr + ind, wc);
}

int
thr_start(struct context *ctx, wchar_t wc)
{
	return ip(ctx->thr)->op(ctx, ctx->thr, wc);
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
pat_do_match(struct pattern *pat, struct context *ctx)
{
	int err = 0;
	size_t max = 0;
	struct thread *it;

	err = pat_exec(ctx);
	if (err) return err;

	vec_foreach (it, ctx->fin) {
		if (it->mat[0].ext > ctx->fin[max].mat[0].ext
		&& it->mat[0].off <= ctx->fin[max].mat[0].off) {
			max = it - ctx->fin;
		}
	}

	return vec_copy(&pat->mat, ctx->fin[max].mat);
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
	if (!vec_len(ctx->fin)) return -1;
	return 0;
}

size_t
pat_lex(struct state *st)
{
	int ret = 0;
	struct token *tok = st->tok;
	struct pos *pos = st->str;

	ret = tok->len = 1;
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
		ret = tok->len = mbtowc(&tok->wc, str(pos), rem(pos));
		if (ret == -1) {
			ret = 1;
			tok->wc = pos->v[0];
		} else if (ret == 0) ret = 1;
	}

	return ret;

esc:
	switch (pos->v[pos->f + 1]) {
	default:
		tok->type = sym_literal;
		ret += tok->len = mbtowc(&tok->wc, str(pos), rem(pos));
		if (ret == -1) {
			ret = 1;
			tok->wc = pos->v[0];
		} else if (ret == 0) ret = 1;
	}

	return ret;
}

int
pat_parse(struct node **rootp, char const *src, struct pattern *pat)
{
	int err = 0;
	struct state st[1] = {{
		.cur = *rootp,
		.str = {{ .v = src, .n = strlen(src), }}
	}};

	vec_ctor(st->buf);
	if (!st->buf) return ENOMEM;

	st->cur->type = type_root;

	while (!eol(st->str)) {
		st->str->f += pat_lex(st);
		err = pat_add[st->tok->type](st, pat);
		if (err) break;
	}

	st_flush(st);
	vec_free(st->buf);

	st_surface(st);

	*rootp = st->cur;

	return err;
}

int
pat_compile(struct pattern *dest, char const *src)
{
	int err = 0;
	struct node *root = 0x0;

	err = vec_ctor(dest->prog);
	if (err) goto finally;

	err = vec_ctor(dest->mat);
	if (err) goto finally;

	root = nod_ctor(0x0);
	if (!root) {
		err = ENOMEM;
		goto finally;
	}

	err = pat_parse(&root, src, dest);
	if (err) goto finally;

	err = pat_marshal(dest, root);
	if (err) goto finally;

finally:
	nod_free(root);
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
pat_match(struct pattern *pat, char const *str)
{
	return pat_match_callback(pat, get_char, (struct pos[]){
			{ .n = strlen(str) + 1, .v = str },
	});
}

int
pat_match_callback(struct pattern *pat, int (*cb)(char *, void *), void *cbx)
{
	struct context ctx[1] = {{.cb = cb, .cbx = cbx}};
	struct thread th[1] = {{ .ins = pat->prog }};
	int err = 0;

	err = ctx_init(ctx);
	if (err) goto finally;

	err = vec_ctor(th->mat);
	if (err) goto finally;

	err = vec_append(&ctx->thr, th);
	if (err) goto finally;

	err = pat_do_match(pat, ctx);
	if (err) goto finally;

finally:
	ctx_fini(ctx);
	return err;
}
