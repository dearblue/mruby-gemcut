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
  uint32_t fixed:1;
  uint32_t module_defined:1;

  bitmap_unit pendings[MGEMS_BITMAP_UNITS];
  bitmap_unit installs[MGEMS_BITMAP_UNITS];
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

static mrb_value
get_gemcut_trial(mrb_state *mrb, mrb_value unused)
{
  mrb_value v = mrb_gv_get(mrb, id_gemcut);
  struct gemcut *gcut = (struct gemcut *)mrb_data_check_get_ptr(mrb, v, &gemcut_type);
  if (gcut) { return mrb_cptr_value(mrb, gcut); }

  struct RData *d = mrb_data_object_alloc(mrb, NULL, NULL, &gemcut_type);
  d->data = mrb_calloc(mrb, 1, sizeof(struct gemcut));
  mrb_gv_set(mrb, id_gemcut, mrb_obj_value(d));

  return mrb_cptr_value(mrb, d->data);
}

MRB_API struct gemcut *
get_gemcut(mrb_state *mrb)
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
  struct gemcut *gcut = get_gemcut(mrb);
  if (gcut == NULL || gcut->fixed) { return; }
  memset(gcut->pendings, 0, sizeof(gcut->pendings));
}

static int
gemcut_pickup(struct gemcut *gcut, const char name[])
{
  /* TODO: すでに含むようになっている mgem であればそのまま回れ右 */

  FOREACH_ALIST(const struct mgem_spec, *mgem, mgems_list) {
    if (strcmp(name, mgem->name) == 0) {
      if (mgem->deps) {
        /* deps を先に再帰的初期化する */
        FOREACH_NLIST(const struct mgem_spec, *d, mgem->numdeps, *mgem->deps) {
          if (gemcut_pickup(gcut, d->name) != 0) {
            return 1;
          }
        }
      }

      int i = mgem - mgems_list;
      gcut->pendings[i / MGEMS_UNIT_BITS] |= 1 << (i % MGEMS_UNIT_BITS);

      return 0;
    }
  }

  return 1;
}

int
mruby_gemcut_pickup(mrb_state *mrb, const char name[])
{
  struct gemcut *gcut = get_gemcut(mrb);
  if (gcut == NULL || gcut->fixed) { return 1; }
  return gemcut_pickup(gcut, name);
}

int
mruby_gemcut_imitate_to(mrb_state *dest, mrb_state *src)
{
  const struct gemcut *gsrc = get_gemcut(src);
  struct gemcut *gdest = get_gemcut(dest);
  if (gsrc == NULL || gdest == NULL || gdest->fixed) { return 1; }

  for (int i = 0; i < MGEMS_BITMAP_UNITS; i ++) {
    gdest->pendings[i] |= gsrc->pendings[i];
  }

  return 0;
}

static int
nopendings(struct gemcut *gcut)
{
  const bitmap_unit *p = gcut->pendings;

  for (int i = MGEMS_BITMAP_UNITS; i > 0; i --, p ++) {
    if (*p != 0) {
      return 0;
    }
  }

  return 1;
}

static void finalization(mrb_state *);

static void
finalization(mrb_state *mrb)
{
  struct gemcut *gcut = get_gemcut(mrb);
  if (gcut == NULL) { return; }

  bitmap_unit ins;
  const struct mgem_spec *mgem = mgems_list + MGEMS_POPULATION - 1;
  for (int i = 0; i < MGEMS_POPULATION; i ++, mgem --, ins >>= 1) {
    if (i % MGEMS_UNIT_BITS == 0) {
      ins = gcut->installs[i / MGEMS_UNIT_BITS];
    }

    if (ins & 1 && mgem->gem_final) {
      /* TODO: mrb_protect でくくる */
      mgem->gem_final(mrb);
    }
  }
}

static mrb_value
gemcut_commit(mrb_state *mrb, mrb_value aa)
{
  struct gemcut *gcut = get_gemcut(mrb);

  if (gcut == NULL) { mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err)); }
  if (gcut->fixed) { mrb_raise(mrb, E_RUNTIME_ERROR, "すでにコミットした状態です"); }

  gcut->fixed = 1;

  if (nopendings(gcut)) {
    return mrb_nil_value();
  }

  mrb_state_atexit(mrb, finalization);

  bitmap_unit pend;
  const struct mgem_spec *mgem = mgems_list;
  for (int i = 0; i < MGEMS_POPULATION; i ++, mgem ++, pend >>= 1) {
    if (i % MGEMS_UNIT_BITS == 0) {
      pend = gcut->pendings[i / MGEMS_UNIT_BITS];
    }

    if (pend & 1) {
      int inv = MGEMS_POPULATION - i - 1;
      gcut->installs[inv / MGEMS_UNIT_BITS] |= 1 << (inv % MGEMS_UNIT_BITS);
      if (mgem->gem_init) {
        mgem->gem_init(mrb);
      }
    }
  }

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
  struct gemcut *g = get_gemcut(mrb);
  if (g == NULL) { mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err)); }
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
  if (g == NULL) { mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err)); }
  if (g->fixed == 0) { mrb_raise(mrb, E_RUNTIME_ERROR, "まだコミットされていません"); }
  mrb_value ary = mrb_ary_new(mrb);
  for (int i = 0; i < MGEMS_POPULATION; i ++) {
    if ((g->installs[i / MGEMS_UNIT_BITS] >> (i % MGEMS_UNIT_BITS)) & 1) {
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
  struct gemcut *g = get_gemcut(mrb);
  if (g == NULL) { mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err)); }
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
  if (g == NULL) { mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err)); }
  if (g->fixed == 0) { mrb_raise(mrb, E_RUNTIME_ERROR, "まだコミットされていません"); }
  int bits = 0;
  for (int i = 0; i < MGEMS_BITMAP_UNITS; i ++) {
    bits += popcount32(g->installs[i]);
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
  if (g == NULL) { mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err)); }
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
  if (g == NULL) { mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err)); }
  if (g->fixed != 0) { return mrb_nil_value(); } /* XXX: 例外の方が嬉しいかな？ */
  int index = gemcut_lookup(g, name);
  if (index < 0) { return mrb_nil_value(); }
  index = MGEMS_BITMAP_UNITS - index - 1;
  if ((g->installs[index / MGEMS_UNIT_BITS] >> (index % MGEMS_UNIT_BITS)) & 1) {
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
  if (g == NULL) { mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err)); }
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
  if (g == NULL) { mrb_exc_raise(mrb, mrb_obj_value(mrb->nomem_err)); }
  g->fixed = 1;

  if (g->module_defined == 0) { gemcut_define_module_trial(mrb, mrb_nil_value()); }
}

void
mrb_mruby_gemcut_gem_final(mrb_state *mrb)
{
}
