#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <util.h>

#include <pat.h>
#include <pat.ih>

struct context {
	size_t         pos;
	struct thread *res;
	struct thread *thr;
	struct thread *que[2];
	struct thread *frl[2];
};

static void ctx_fini(struct context *);
static int  ctx_init(struct context *, struct pattern *);
static int  ctx_next(struct context *, char const);
static void ctx_prune(struct context *);
static void ctx_rm(struct context *);
static void ctx_shift(struct context *);
static int  ctx_step(struct context *, char const);

static struct thread *ctx_get(struct context *);

static int pat_exec(struct context *, int (*)(char *, void *), void *);

struct thread *
ctx_get(struct context *ctx)
{
	struct thread *ret;

	if (!ctx->frl[0] && thr_alloc(ctx->frl)) {
		return 0;
	}

	ret = ctx->frl[0];
	if (ctx->frl[0] == ctx->frl[1]) {
		ctx->frl[1] = 0;
	}
	ctx->frl[0] = ctx->frl[0]->next;
	ret->next = 0x0;

	return ret;
}

void
ctx_fini(struct context *ctx)
{
	thr_free(ctx->thr);
	thr_free(ctx->que[0]);
	thr_free(ctx->frl[0]);
	thr_free(ctx->res);
}

int
ctx_init(struct context *ctx, struct pattern *pat)
{
	int err = 0;

	ctx->que[0] = ctx_get(ctx);
	if (!ctx->que[0]) return ENOMEM;

	err = thr_init(ctx->que[0], pat->prog);
	if (err) goto fail;

	return 0;

fail:
	ctx_fini(ctx);
	return err;
}

int
ctx_next(struct context *ctx, char const ch)
{
	ctx_prune(ctx); 
	if (!ctx->thr) return 0;
	return ctx_step(ctx, ch);
}

void
ctx_prune(struct context *ctx)
{
	while (ctx->thr) {
		if (thr_cmp(ctx->res, ctx->thr) < 0) {
			break;
		} else ctx_rm(ctx);
	}
}

void
ctx_rm(struct context *ctx)
{
	thr_mv(ctx->frl, &ctx->thr);
}

void
ctx_que(struct context *ctx)
{
	thr_mv(ctx->que, &ctx->thr);
}

void
ctx_shift(struct context *ctx)
{
	ctx->thr = ctx->que[0];
	ctx->que[0] = 0;
	ctx->que[1] = 0;
}

int
ctx_step(struct context *ctx, char const ch)
{
	return ctx->thr->ip->op(ctx, ch);
}

int
do_char(struct context *ctx, char const ch)
{
	if (ctx->thr->ip->arg.b == ch) {
		++ctx->thr->ip;
		ctx_que(ctx);
	} else ctx_rm(ctx);

	return ctx_next(ctx, ch);
}

int
do_clss(struct context *ctx, char const ch)
{
	bool res;

	switch (ctx->thr->ip->arg.b) {
	case 0: res = true; break;
	case '.': res = ch != L'\n' && ch != L'\0'; break;
	}

	if (res) {
		++ctx->thr->ip;
		ctx_que(ctx);
	}
	else ctx_rm(ctx);

	return ctx_next(ctx, ch);
}

int
do_fork(struct context *ctx, char const ch)
{
	struct thread *new;
	int err = 0;

	new = ctx_get(ctx);
	if (!new) return ENOMEM;

	err = thr_fork(new, ctx->thr);
	if (err) goto fail;

	new->ip += ctx->thr->ip->arg.f;
	++ctx->thr->ip;

	new->next = ctx->thr;
	ctx->thr = new;

	return ctx->thr->ip->op(ctx, ch);

fail:
	thr_free(new);
	return err;
}

int
do_halt(struct context *ctx, char const ch)
{
	struct thread *th;

	if (thr_cmp(ctx->res, ctx->thr) > 0) {
		ctx_rm(ctx);
		return ctx_next(ctx, ch);
	}

	// abstract
	if (ctx->res) thr_mv(ctx->frl, &ctx->res);
	th = ctx->thr;
	ctx->thr = ctx->thr->next;
	ctx->res = th;
	ctx->res->next = 0;
	return ctx_next(ctx, ch);
}

int
do_jump(struct context *ctx, char const ch)
{
	ctx->thr->ip += ctx->thr->ip->arg.f;
	return ctx->thr->ip->op(ctx, ch);
}

int
do_mark(struct context *ctx, char const ch)
{
	struct thread *th = ctx->thr;

	if (th->nmat < 10) {
		th->mat[th->nmat++] = (struct patmatch){ ctx->pos, -1 };
	}

	++th->ip;
	return th->ip->op(ctx, ch);
}

int
do_save(struct context *ctx, char const ch)
{
	struct thread *th = ctx->thr;
	size_t off = th->nmat;

	while (th->mat[--off].ext != -1UL) continue;

	th->mat[off].ext = ctx->pos - th->mat[off].off;
	++th->ip;
	return th->ip->op(ctx, ch);
}

int
pat_exec(struct context *ctx, int (*cb)(char *, void *), void *cbx)
{
	int err;
	char c;

	while (cb(&c, cbx)) {

		ctx_shift(ctx);	

		err = ctx_next(ctx, c);
		if (err) break;

		++ctx->pos;
	}
	
	if (err) return err;

	return 0;
}

int
pat_match(struct pattern *pat, int (*cb)(char *, void *), void *cbx)
{
	struct context ctx[1] = {{0}};
	int err;

	err = ctx_init(ctx, pat);
	if (err) return err;
	
	err = pat_exec(ctx, cb, cbx);
	if (err) goto finally;

	if (!ctx->res) return -1;

finally:
	memcpy(pat->mat, ctx->res->mat, sizeof pat->mat);
	pat->nmat = ctx->res->nmat;

	ctx_fini(ctx);

	return err;
}
