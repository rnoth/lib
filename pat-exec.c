#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <vec.h>
#include <util.h>
#include <pat.h>
#include <pat.ih>

void
ctx_fini(struct context *ctx)
{
	struct thread *th;

	vec_foreach(th, ctx->thr) vec_free(th->mat);
	vec_free(ctx->thr);
	vec_free(ctx->fin->mat);
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

int
do_char(struct context *ctx, struct thread *th, char const ch)
{
	size_t ind = th - ctx->thr;

	if (th->ip->arg.b != ch) thr_remove(ctx, th - ctx->thr), --ind;
	else ++th->ip;

	return thr_next(ctx, ind + 1, ch);
}

int
do_clss(struct context *ctx, struct thread *th, char const ch)
{
	size_t ind = th - ctx->thr;
	bool res;

	switch (th->ip->arg.b) {
	case 0: res = true; break;
	case '.': res = ch != L'\n' && ch != L'\0'; break;
	}

	if (!res) thr_remove(ctx, ind), --ind;
	else ++th->ip;

	return thr_next(ctx, ind + 1, ch);
}

int
do_fork(struct context *ctx, struct thread *th, char const ch)
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
	return th->ip->op(ctx, th, ch);

fail:
	vec_free(new->mat);
	return err;
}

int
do_halt(struct context *ctx, struct thread *th, char const ch)
{
	struct patmatch term[] = {{ .off = -1, .ext = -1 }};
	size_t ind = th - ctx->thr;
	int err;

	if (thr_cmp(ctx->fin, th) > 0) {
		thr_remove(ctx, ind);
		return thr_next(ctx, ind, ch);
	}

	err = vec_append(&th->mat, term);
	if (err) return err;

	thr_finish(ctx, ind);
	return thr_next(ctx, ind, ch);
}

int
do_jump(struct context *ctx, struct thread *th, char const ch)
{
	th->ip += th->ip->arg.f;
	return th->ip->op(ctx, th, ch);
}

int
do_mark(struct context *ctx, struct thread *th, char const ch)
{
	int err = 0;
	struct patmatch mat = {0};

	mat.off = ctx->pos;
	mat.ext = -1;

	err = vec_append(&th->mat, &mat);
	if (err) return err;

	++th->ip;
	return th->ip->op(ctx, th, ch);
}

int
do_save(struct context *ctx, struct thread *th, char const ch)
{
	size_t off = vec_len(th->mat);

	while (off --> 0) if (th->mat[off].ext == (size_t)-1) break;

	th->mat[off].ext = ctx->pos - th->mat[off].off;

	th->ip++;
	return th->ip->op(ctx, th, ch);
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


