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
  int fixed;
  uint32_t pendings[MGEMS_BITMAP_UNITS];
  uint32_t installs[MGEMS_BITMAP_UNITS];
};

#define id_gemcut mrb_intern_lit(mrb, "mruby-gemcut-structure")

static const mrb_data_type gemcut_type = { "mruby-gemcut", mrb_free };

static struct gemcut *
get_gemcut(mrb_state *mrb)
{
  mrb_value v = mrb_gv_get(mrb, id_gemcut);
  struct gemcut *gcut = (struct gemcut *)mrb_data_check_get_ptr(mrb, v, &gemcut_type);
  if (gcut) { return gcut; }

  struct RData *d = mrb_data_object_alloc(mrb, NULL, NULL, &gemcut_type);
  d->data = mrb_calloc(mrb, 1, sizeof(struct gemcut));
  mrb_gv_set(mrb, id_gemcut, mrb_obj_value(d));

  return (struct gemcut *)d->data;
}

MRB_API void
mruby_gemcut_clear(mrb_state *mrb)
{
  struct gemcut *gcut = get_gemcut(mrb);
  if (gcut->fixed) { return; }
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
      gcut->pendings[i / 32] |= 1 << (i % 32);

      return 0;
    }
  }

  return 1;
}

int
mruby_gemcut_pickup(mrb_state *mrb, const char name[])
{
  struct gemcut *gcut = get_gemcut(mrb);
  if (gcut->fixed) { mrb_raise(mrb, E_RUNTIME_ERROR, "すでにコミットした状態です"); }
  return gemcut_pickup(gcut, name);
}

int
mruby_gemcut_imitate_to(mrb_state *dest, mrb_state *src)
{
  const struct gemcut *gsrc = get_gemcut(src);
  struct gemcut *gdest = get_gemcut(dest);
  if (gdest->fixed) { return 1; }

  for (int i = 0; i < MGEMS_BITMAP_UNITS; i ++) {
    gdest->pendings[i] |= gsrc->pendings[i];
  }

  return 0;
}

static int
nopendings(struct gemcut *gcut)
{
  const uint32_t *p = gcut->pendings;

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

  uint32_t ins;
  const struct mgem_spec *mgem = mgems_list + MGEMS_POPULATION - 1;
  for (int i = 0; i < MGEMS_POPULATION; i ++, mgem --, ins >>= 1) {
    if (i % 32 == 0) {
      ins = gcut->installs[i / 32];
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

  if (gcut->fixed) { mrb_raise(mrb, E_RUNTIME_ERROR, "すでにコミットした状態です"); }

  gcut->fixed = 1;

  if (nopendings(gcut)) {
    return mrb_nil_value();
  }

  mrb_state_atexit(mrb, finalization);

  uint32_t pend;
  const struct mgem_spec *mgem = mgems_list;
  for (int i = 0; i < MGEMS_POPULATION; i ++, mgem ++, pend >>= 1) {
    if (i % 32 == 0) {
      pend = gcut->pendings[i / 32];
    }

    if (pend & 1) {
      int inv = MGEMS_POPULATION - i - 1;
      gcut->installs[inv / 32] |= 1 << (inv % 32);
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

void
mrb_mruby_gemcut_gem_init(mrb_state *mrb)
{
  /*
   * この関数が呼ばれるのは `mrb_open()` / `mrb_open_alloc()` か
   * `mruby-gemcut`、あるいは似たような mruby-gems からだと思われるため、
   * 二重初期化を避ける意味でこれ以降の `mruby_gemcut_commit()` を禁止する。
   */

  get_gemcut(mrb)->fixed = 1;

#if 0
  /* 以下を追加するかもしれない */
  struct RClass *gemcut = mrb_define_module(mrb, "GemCut");
  mrb_define_class_method(mrb, gemcut, "availables", gemcut_availables, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, gemcut, "installs", gemcut_pickups, MRB_ARGS_NONE());
#endif
}

void
mrb_mruby_gemcut_gem_final(mrb_state *mrb)
{
}
