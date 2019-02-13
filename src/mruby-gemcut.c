#include "internals.h"
#include <string.h>

#define FOREACH_ALIST(T, V, L)                                              \
        for (T V = (L), *_end_ = (L) + sizeof(L) / sizeof((L)[0]);          \
             &V < _end_;                                                    \
             &V ++)                                                         \

#define FOREACH_NLIST(T, V, N, L)                                           \
        for (T V = (L), *_end_ = (L) + (N);                                 \
             &V < _end_;                                                    \
             &V ++)                                                         \

static inline int
popcount32(uint32_t n)
{
  n = (n & 0x55555555UL) + ((n >>  1) & 0x55555555UL);
  n = (n & 0x33333333UL) + ((n >>  2) & 0x33333333UL);
  n = (n + (n >> 4)) & 0x0f0f0f0fUL; /* 4 + 4 = 8 が最大なので、加算前のビットマスクは不要 */
  n += n >>  8; /* 以降は 0..32 に収まるため、ビットマスクは不要 */
  n += n >> 16;
  return n & 0xff;
}

struct mgem_spec
{
  const char *name;
  void (*gem_init)(mrb_state *mrb);
  void (*gem_final)(mrb_state *mrb);
  const int numdeps;
  const struct mgem_spec *const *deps;
};

/*
 * HINT:
 *      `<build>/mrbgems/mruby-gemcut/include/mruby-gemcut/deps.h` は
 *      `mruby-gemcut/mrbgem.rake` によって構成ごとに生成される
 */
#include <mruby-gemcut/deps.h>

struct gemcut
{
  uint32_t gems_committed:1;
  uint32_t module_defined:1;

  bitmap_unit pickups[MGEMS_BITMAP_UNITS];
  bitmap_unit commits[MGEMS_BITMAP_UNITS];
};

static int
gemcut_lookup(const struct gemcut *g, const char name[])
{
  FOREACH_ALIST(const struct mgem_spec, *mgem, mgems_list) {
    if (strcmp(name, mgem->name) == 0) {
      return mgem - mgems_list;
    }
  }

  return -1;
}

#define id_gemcut mrb_intern_lit(mrb, "mruby-gemcut-structure")

static const mrb_data_type gemcut_type = { "mruby-gemcut", mrb_free };

static struct gemcut *
get_gemcut(mrb_state *mrb)
{
  mrb_value v = mrb_gv_get(mrb, id_gemcut);
  struct gemcut *gcut = (struct gemcut *)mrb_data_check_get_ptr(mrb, v, &gemcut_type);

  if (gcut == NULL) {
    struct RData *d = mrb_data_object_alloc(mrb, NULL, NULL, &gemcut_type);
    d->data = mrb_calloc(mrb, 1, sizeof(struct gemcut));
    mrb_gv_set(mrb, id_gemcut, mrb_obj_value(d));
    gcut = (struct gemcut *)d->data;
  }

  return gcut;
}

static mrb_value
get_gemcut_trial(mrb_state *mrb, mrb_value unused)
{
  return mrb_cptr_value(mrb, get_gemcut(mrb));
}

static struct gemcut *
get_gemcut_noraise(mrb_state *mrb)
{
  mrb_bool state;
  mrb_value ret = mrb_protect(mrb, get_gemcut_trial, mrb_nil_value(), &state);
  if (state || mrb_type(ret) != MRB_TT_CPTR) {
    return NULL;
  } else {
    return (struct gemcut *)mrb_cptr(ret);
  }
}

MRB_API void
mruby_gemcut_clear(mrb_state *mrb)
{
  struct gemcut *gcut = get_gemcut_noraise(mrb);
  if (gcut == NULL || gcut->gems_committed) { return; }
  memset(gcut->pickups, 0, sizeof(gcut->pickups));
}

static int
gemcut_pickup(struct gemcut *gcut, const char name[])
{
  int gi = gemcut_lookup(gcut, name);
  if (gi < 0) { return 1; }
  if (gcut->pickups[gi / MGEMS_UNIT_BITS] & (1 << gi % MGEMS_UNIT_BITS)) {
    return 0;
  }

  const struct mgem_spec *mgem = &mgems_list[gi];

  if (mgem->deps) {
    /* deps を先に再帰的初期化する */
    FOREACH_NLIST(const struct mgem_spec, *d, mgem->numdeps, *mgem->deps) {
      if (gemcut_pickup(gcut, d->name) != 0) {
        return 1;
      }
    }
  }

  gcut->pickups[gi / MGEMS_UNIT_BITS] |= 1 << (gi % MGEMS_UNIT_BITS);

  return 0;
}

int
mruby_gemcut_pickup(mrb_state *mrb, const char name[])
{
  struct gemcut *gcut = get_gemcut_noraise(mrb);
  if (gcut == NULL || gcut->gems_committed) { return 1; }
  return gemcut_pickup(gcut, name);
}

int
mruby_gemcut_imitate_to(mrb_state *dest, mrb_state *src)
{
  const struct gemcut *gsrc = get_gemcut_noraise(src);
  struct gemcut *gdest = get_gemcut_noraise(dest);
  if (gsrc == NULL || gdest == NULL || gdest->gems_committed) { return 1; }

  for (int i = 0; i < MGEMS_BITMAP_UNITS; i ++) {
    gdest->pickups[i] |= gsrc->pickups[i];
  }

  return 0;
}

static int
nopickups(struct gemcut *gcut)
{
  const bitmap_unit *p = gcut->pickups;

  for (int i = MGEMS_BITMAP_UNITS; i > 0; i --, p ++) {
    if (*p != 0) {
      return 0;
    }
  }

  return 1;
}

static mrb_value gem_final_trial(mrb_state *, mrb_value);
static void finalization(mrb_state *);

static mrb_value
gem_final_trial(mrb_state *mrb, mrb_value opaque)
{
  const struct mgem_spec *mgem = (const struct mgem_spec *)mrb_cptr(opaque);
  mgem->gem_final(mrb);
  return mrb_nil_value();
}

static void
finalization(mrb_state *mrb)
{
  struct gemcut *gcut = get_gemcut_noraise(mrb);
  if (gcut == NULL) { return; }

  bitmap_unit comts;
  const struct mgem_spec *mgem = mgems_list + MGEMS_POPULATION - 1;
  int ai = mrb_gc_arena_save(mrb);
  for (int i = 0; i < MGEMS_POPULATION; i ++, mgem --, comts >>= 1) {
    if (i % MGEMS_UNIT_BITS == 0) {
      comts = gcut->commits[i / MGEMS_UNIT_BITS];
    }

    if (comts & 1 && mgem->gem_final) {
      mrb_protect(mrb, gem_final_trial, mrb_cptr_value(mrb, (void *)mgem), NULL);
      mrb_gc_arena_restore(mrb, ai);
    }
  }
}

/* これらの具体的な数値は mruby-fiber からのパクリ */
enum {
  context_stack_size = 64,
  context_ci_size = 8,
};

static struct mrb_context *
alloc_context(mrb_state *mrb)
{
  /*
   * XXX: はたしてバージョン間における互換性がどれだけ保たれることやら……。
   *      mruby 1.2, 1.3, 1.4, 1.4.1, 2.0 は host/bin/mruby-gemcut-test が動作することを確認。
   */
  struct mrb_context *c = (struct mrb_context *)mrb_calloc(mrb, 1, sizeof(struct mrb_context));
  c->stbase = (mrb_value *)mrb_malloc_simple(mrb, context_stack_size * sizeof(*c->stbase));
  if (c->stbase == NULL) { goto nomem; }
  /* c->stbase の初期化は setup_context() で行うため省略 */
  c->stend = c->stbase + context_stack_size;
  c->stack = c->stbase;
  c->cibase = (mrb_callinfo *)mrb_malloc_simple(mrb, context_ci_size * sizeof(*c->cibase));
  if (c->cibase == NULL) { goto nomem; }
  memset(c->cibase, 0, context_ci_size * sizeof(*c->cibase));
  c->ciend = c->cibase + context_ci_size;
  c->ci = c->cibase;
  c->ci->stackent = c->stack;
  //c->ci->proc = mrb->c->ci->proc;
  c->ci->target_class = mrb->object_class;
  //c->fib = mrb->c->fib;

  return c;

nomem:
  mrb_free_context(mrb, c);
  mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err));
  return NULL; /* not reached here */
}

static void
setup_context(mrb_state *mrb, struct mrb_context *c)
{
  for (mrb_value *p = c->stbase; p < c->stend; p ++) {
    *p = mrb_nil_value();
  }

  mrb->c = c;
}

struct gemcut_commit_restore
{
  struct gemcut *gcut;
  struct mrb_context *orig_c;
  struct mrb_context *work_c;
};

static mrb_value
gemcut_commit_trial(mrb_state *mrb, mrb_value args)
{
  const struct gemcut_commit_restore *p = (struct gemcut_commit_restore *)mrb_cptr(args);
  int ai = mrb_gc_arena_save(mrb);
  bitmap_unit picks;
  const struct mgem_spec *mgem = mgems_list;
  for (int i = 0; i < MGEMS_POPULATION; i ++, mgem ++, picks >>= 1) {
    if (i % MGEMS_UNIT_BITS == 0) {
      picks = p->gcut->pickups[i / MGEMS_UNIT_BITS];
    }

    if (picks & 1) {
      int inv = MGEMS_POPULATION - i - 1;
      p->gcut->commits[inv / MGEMS_UNIT_BITS] |= 1 << (inv % MGEMS_UNIT_BITS);
      if (mgem->gem_init) {
        setup_context(mrb, p->work_c);
        mgem->gem_init(mrb);
        mrb_gc_arena_restore(mrb, ai);
      }
    }
  }

  return mrb_nil_value();
}

static mrb_value
gemcut_commit_restore(mrb_state *mrb, mrb_value args)
{
  const struct gemcut_commit_restore *p = (struct gemcut_commit_restore *)mrb_cptr(args);
  mrb->c = p->orig_c;
  mrb_free_context(mrb, p->work_c);

  return mrb_nil_value();
}

static mrb_value
gemcut_commit(mrb_state *mrb, mrb_value aa)
{
  struct gemcut *gcut = get_gemcut(mrb);

  if (gcut->gems_committed) { mrb_raise(mrb, E_RUNTIME_ERROR, "すでにコミットした状態です"); }

  gcut->gems_committed = 1;

  if (nopickups(gcut)) {
    return mrb_nil_value();
  }

  mrb_state_atexit(mrb, finalization);

  struct gemcut_commit_restore restore = { gcut, mrb->c, NULL };
  mrb_value restorev = mrb_cptr_value(mrb, (void *)&restore);
  restore.work_c = alloc_context(mrb);
  mrb_ensure(mrb, gemcut_commit_trial, restorev, gemcut_commit_restore, restorev);
  return mrb_nil_value();
}

struct RException *
mruby_gemcut_commit(mrb_state *mrb)
{
  mrb_bool state;
  mrb_value ret = mrb_protect(mrb, gemcut_commit, mrb_nil_value(), &state);

  if (state) {
    if (mrb_type(ret) == MRB_TT_EXCEPTION) { return mrb_exc_ptr(ret); }
  }

  return NULL;
}

static mrb_value
gemcut_available_list_trial(mrb_state *mrb, mrb_value unused)
{
  (void)get_gemcut(mrb);
  mrb_value ary = mrb_ary_new_capa(mrb, MGEMS_POPULATION);
  for (int i = 0; i < MGEMS_POPULATION; i ++) {
    mrb_ary_push(mrb, ary, mrb_str_new_static(mrb, mgems_list[i].name, strlen(mgems_list[i].name)));
  }
  return ary;
}

MRB_API mrb_value
mruby_gemcut_available_list(mrb_state *mrb)
{
  mrb_bool state;
  mrb_value ret = mrb_protect(mrb, gemcut_available_list_trial, mrb_nil_value(), &state);
  if (state != 0 || mrb_type(ret) != MRB_TT_ARRAY) {
    return mrb_nil_value();
  } else {
    return ret;
  }
}

static mrb_value
gemcut_committed_list_trial(mrb_state *mrb, mrb_value unused)
{
  struct gemcut *g = get_gemcut(mrb);
  if (g->gems_committed == 0) { mrb_raise(mrb, E_RUNTIME_ERROR, "まだコミットされていません"); }
  mrb_value ary = mrb_ary_new(mrb);
  for (int i = 0; i < MGEMS_POPULATION; i ++) {
    if ((g->commits[i / MGEMS_UNIT_BITS] >> (i % MGEMS_UNIT_BITS)) & 1) {
      const struct mgem_spec *e = &mgems_list[MGEMS_POPULATION - i - 1];
      mrb_ary_push(mrb, ary, mrb_str_new_static(mrb, e->name, strlen(e->name)));
    }
  }
  return ary;
}

MRB_API mrb_value
mruby_gemcut_committed_list(mrb_state *mrb)
{
  mrb_bool state;
  mrb_value ret = mrb_protect(mrb, gemcut_committed_list_trial, mrb_nil_value(), &state);
  if (state != 0 || mrb_type(ret) != MRB_TT_ARRAY) {
    return mrb_nil_value();
  } else {
    return ret;
  }
}

static mrb_value
gemcut_available_size_trial(mrb_state *mrb, mrb_value opaque)
{
  (void)get_gemcut(mrb);
  return mrb_fixnum_value(MGEMS_POPULATION);
}

MRB_API int
mruby_gemcut_available_size(mrb_state *mrb)
{
  mrb_bool state;
  mrb_value ret = mrb_protect(mrb, gemcut_available_size_trial, mrb_nil_value(), &state);
  if (state != 0 && !mrb_fixnum_p(ret)) {
    return -1;
  } else {
    return mrb_fixnum(ret);
  }
}

static mrb_value
gemcut_commit_size_trial(mrb_state *mrb, mrb_value opaque)
{
  struct gemcut *g = get_gemcut(mrb);
  if (g->gems_committed == 0) { mrb_raise(mrb, E_RUNTIME_ERROR, "まだコミットされていません"); }
  int bits = 0;
  for (int i = 0; i < MGEMS_BITMAP_UNITS; i ++) {
    bits += popcount32(g->commits[i]);
  }
  return mrb_fixnum_value(bits);
}

MRB_API int
mruby_gemcut_commit_size(mrb_state *mrb)
{
  mrb_bool state;
  mrb_value ret = mrb_protect(mrb, gemcut_commit_size_trial, mrb_nil_value(), &state);
  if (state != 0 && !mrb_fixnum_p(ret)) {
    return -1;
  } else {
    return mrb_fixnum(ret);
  }
}

static mrb_value
gemcut_available_p_trial(mrb_state *mrb, mrb_value opaque)
{
  const char *name = (const char *)mrb_cptr(opaque);
  struct gemcut *g = get_gemcut(mrb);
  int index = gemcut_lookup(g, name);
  if (index < 0) {
    return mrb_false_value();
  } else {
    return mrb_true_value();
  }
}

MRB_API mrb_bool
mruby_gemcut_available_p(mrb_state *mrb, const char name[])
{
  mrb_bool state;
  mrb_value ret = mrb_protect(mrb, gemcut_available_p_trial, mrb_cptr_value(mrb, (void *)(uintptr_t)name), &state);
  if (state == 0 && !mrb_nil_p(ret)) {
    switch (mrb_type(ret)) {
    case MRB_TT_TRUE:
      return TRUE;
    case MRB_TT_FALSE:
      return FALSE;
    default:
      break;
    }
  }

  return FALSE;
}

static mrb_value
gemcut_committed_p_trial(mrb_state *mrb, mrb_value opaque)
{
  const char *name = (const char *)mrb_cptr(opaque);
  struct gemcut *g = get_gemcut(mrb);
  if (g->gems_committed == 0) { return mrb_nil_value(); } /* XXX: 例外の方が嬉しいかな？ */
  int index = gemcut_lookup(g, name);
  if (index < 0) { return mrb_nil_value(); }
  index = MGEMS_BITMAP_UNITS - index - 1;
  if ((g->commits[index / MGEMS_UNIT_BITS] >> (index % MGEMS_UNIT_BITS)) & 1) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

MRB_API mrb_bool
mruby_gemcut_committed_p(mrb_state *mrb, const char name[])
{
  mrb_bool state;
  mrb_value ret = mrb_protect(mrb, gemcut_committed_p_trial, mrb_cptr_value(mrb, (void *)(uintptr_t)name), &state);
  if (state == 0 && !mrb_nil_p(ret)) {
    switch (mrb_type(ret)) {
    case MRB_TT_TRUE:
      return TRUE;
    case MRB_TT_FALSE:
      return FALSE;
    default:
      break;
    }
  }

  return FALSE;
}

static mrb_value
gemcut_s_pickup(mrb_state *mrb, mrb_value self)
{
  const char *name;
  mrb_get_args(mrb, "z", &name);
  int ret = mruby_gemcut_pickup(mrb, name);
  return mrb_bool_value(!ret);
}

static mrb_value
gemcut_s_commit(mrb_state *mrb, mrb_value self)
{
  mrb_get_args(mrb, "");
  gemcut_commit(mrb, mrb_nil_value());
  return mrb_nil_value();
}

static mrb_value
gemcut_available_p(mrb_state *mrb, mrb_value self)
{
  const char *name;
  mrb_get_args(mrb, "z", &name);
  return gemcut_available_p_trial(mrb, mrb_cptr_value(mrb, (void *)name));
}

static mrb_value
gemcut_committed_p(mrb_state *mrb, mrb_value self)
{
  const char *name;
  mrb_get_args(mrb, "z", &name);
  return gemcut_committed_p_trial(mrb, mrb_cptr_value(mrb, (void *)name));
}

static mrb_value
gemcut_define_module_trial(mrb_state *mrb, mrb_value opaque)
{
  struct gemcut *g = get_gemcut(mrb);
  if (g->module_defined) { mrb_raise(mrb, E_RUNTIME_ERROR, "GemCut モジュールはすでに定義されています"); }

  struct RClass *gemcut_mod = mrb_define_module(mrb, "GemCut");
  mrb_define_class_method(mrb, gemcut_mod, "pickup", gemcut_s_pickup, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, gemcut_mod, "commit", gemcut_s_commit, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gemcut_mod, "available_list", gemcut_available_list_trial, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gemcut_mod, "committed_list", gemcut_committed_list_trial, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gemcut_mod, "available_sise", gemcut_available_size_trial, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gemcut_mod, "commit_size", gemcut_commit_size_trial, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gemcut_mod, "available?", gemcut_available_p, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, gemcut_mod, "committed?", gemcut_committed_p, MRB_ARGS_REQ(1));

  return mrb_nil_value();
}

MRB_API int
mruby_gemcut_define_module(mrb_state *mrb)
{
  mrb_bool state;
  mrb_value ret = mrb_protect(mrb, gemcut_define_module_trial, mrb_nil_value(), &state);
  if (state != 0 || !mrb_nil_p(ret)) {
    return 1;
  } else {
    return 0;
  }
}

void
mrb_mruby_gemcut_gem_init(mrb_state *mrb)
{
  /*
   * この関数が呼ばれるのは `mrb_open()` / `mrb_open_alloc()` か
   * `mruby-gemcut`、あるいは似たような mruby-gems からだと思われるため、
   * 二重初期化を避ける意味でこれ以降の `mruby_gemcut_commit()` を禁止する。
   */

  struct gemcut *g = get_gemcut(mrb);
  g->gems_committed = 1;

  if (g->module_defined == 0) { gemcut_define_module_trial(mrb, mrb_nil_value()); }
}

void
mrb_mruby_gemcut_gem_final(mrb_state *mrb)
{
}
