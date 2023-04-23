#include "internals.h"
#include <stdbool.h>
#include <string.h>
#include <mruby/irep.h> /* for mrb_load_irep() */
#include <mruby/dump.h> /* for bin_to_uint32() */

#define FOREACH_ALIST(T, V, L)                                              \
        for (T V = (L), *_end_ = (L) + sizeof(L) / sizeof((L)[0]);          \
             &V < _end_;                                                    \
             &V++)                                                          \

#define FOREACH_NLIST(T, V, N, L)                                           \
        for (T V = (L), *_end_ = (L) + (N);                                 \
             &V < _end_;                                                    \
             &V++)                                                          \

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

#define DEFINE_PROTECTED_FUNCTION(DECL, CALLER, ARG, EXTRACTOR, ERR)    \
  DECL                                                                  \
  {                                                                     \
    if (mrb->jmp) {                                                     \
      mrb_value ret = CALLER(mrb, (void *)(uintptr_t)ARG);              \
      (void)ret;                                                        \
      return EXTRACTOR(ret);                                            \
    } else {                                                            \
      mrb_bool err;                                                     \
      mrb_value ret = mrb_protect_error(mrb, CALLER, (void *)(uintptr_t)ARG, &err); \
      (void)ret;                                                        \
      if (err) {                                                        \
        return ERR;                                                     \
      } else {                                                          \
        return EXTRACTOR(ret);                                          \
      }                                                                 \
    }                                                                   \
  }                                                                     \

#define RESULT_PASSTHROUGH(V) (V)
#define RESULT_VOID(V)
#define RESULT_VOID_ERROR
#define RESULT_TO_ZERO(V) 0

struct mgem_spec
{
  const char *name;
  void (*gem_init)(mrb_state *mrb);
  void (*gem_final)(mrb_state *mrb);
  mrb_bool available:1;
  const uint32_t numdeps:16;
  const uint16_t *deps;
};

#ifndef MRB_PRESYM_SCANNING
/*
 * HINT:
 *      `<build>/mrbgems/mruby-gemcut/include/mruby-gemcut/deps.h` は
 *      `mruby-gemcut/mrbgem.rake` によって構成ごとに生成される
 */
#include <mruby-gemcut/deps.h>
#endif

#if MRUBY_RELEASE_NO < 30000 || defined(MRB_NO_PRESYM)
# define NO_PRESYM(...) do { __VA_ARGS__; } while (0)
#else
# define NO_PRESYM(...) do { } while (0)
#endif

enum gemcut_status {
  gemcut_normal = 0,
  gemcut_locked = 1,
  gemcut_sealed = 2,
};

struct gemcut
{
  bool set_atexit:1;
  bool defined_module:1;
  enum gemcut_status status:2;
  bitmap_unit loaded[MGEMS_BITMAP_UNITS];
};

static bool
gemcut_loaded_p_by_id(const struct gemcut *g, int id)
{
  mrb_assert(id < MGEMS_POPULATION);

  if (id < 0) {
    return false;
  }

  int inv = MGEMS_POPULATION - id - 1;

  return ((g->loaded[inv / MGEMS_UNIT_BITS] >> (inv % MGEMS_UNIT_BITS)) & 1) ? true : false;
}

static void
gemcut_set_loaded_by_id(struct gemcut *g, int id)
{
  mrb_assert(id >= 0 && id < MGEMS_POPULATION);

  int inv = MGEMS_POPULATION - id - 1;
  g->loaded[inv / MGEMS_UNIT_BITS] |= 1UL << (inv % MGEMS_UNIT_BITS);
}

static int
gemcut_lookup(const char name[])
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
get_gemcut_main(mrb_state *mrb, struct gemcut **gcutp)
{
  mrb_value v = mrb_gv_get(mrb, id_gemcut);
  *gcutp = (struct gemcut *)mrb_data_check_get_ptr(mrb, v, &gemcut_type);

  if (*gcutp == NULL) {
    int ai = mrb_gc_arena_save(mrb);

    mrb_intern_lit(mrb, "LoadError");
    mrb_define_class(mrb, "LoadError", mrb_class_get(mrb, "ScriptError"));

    struct RData *d = mrb_data_object_alloc(mrb, NULL, NULL, &gemcut_type);
    d->data = mrb_calloc(mrb, 1, sizeof(struct gemcut));
    mrb_gv_set(mrb, id_gemcut, mrb_obj_value(d));
    *gcutp = (struct gemcut *)d->data;
    mrb_gc_arena_restore(mrb, ai);
    v = mrb_obj_value(d);
  }

  return v;
}

static struct gemcut *
get_gemcut(mrb_state *mrb)
{
  struct gemcut *gcut;
  get_gemcut_main(mrb, &gcut);
  return gcut;
}

static struct gemcut *
get_gemcut_noraise(mrb_state *mrb)
{
  struct gemcut *gcut;
  mrb_bool state;
  mrb_protect_error(mrb, (mrb_value (*)(mrb_state *, void *))get_gemcut_main, &gcut, &state);
  if (state) {
    return NULL;
  } else {
    return gcut;
  }
}

static mrb_noreturn void
gemcut_sealed_error(mrb_state *mrb)
{
  mrb_raise(mrb, mrb->eException_class, "currently feature is sealed"); // SecurityError
}

static void
gemcut_check_sealed(mrb_state *mrb)
{
  if (get_gemcut(mrb)->status == gemcut_sealed) {
    gemcut_sealed_error(mrb);
  }
}

static mrb_value
gemcut_imitate_to_main(mrb_state *dest, void *opaque)
{
  mrb_state *src = (mrb_state *)opaque;
  const struct gemcut *gsrc = get_gemcut(src);
  struct gemcut *gdest = get_gemcut(dest);
  if (gdest->status != gemcut_normal) {
    gemcut_sealed_error(dest);
  }

  for (int i = 0; i < MGEMS_POPULATION; i++) {
    if (gemcut_loaded_p_by_id(gsrc, i) && !gemcut_loaded_p_by_id(gdest, i)) {
      // TODO: mruby_gemcut_require() ではなくて直接 gemcut_require_by_id_main_top() を呼び出すようにする
      mruby_gemcut_require(dest, mgems_list[i].name);
    }
  }

  return mrb_nil_value();
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API mrb_value mruby_gemcut_imitate_to(mrb_state *mrb, mrb_state *src),
    gemcut_imitate_to_main, src, RESULT_PASSTHROUGH, ret)

static mrb_value
gemcut_cleanup_main(mrb_state *mrb, void *opaque)
{
  const struct mgem_spec *mgem = (const struct mgem_spec *)opaque;
  mgem->gem_final(mrb);
  return mrb_nil_value();
}

static void
gemcut_cleanup(mrb_state *mrb)
{
  struct gemcut *gcut = get_gemcut_noraise(mrb);
  if (gcut == NULL) { return; }

  const struct mgem_spec *mgem = mgems_list + MGEMS_POPULATION - 1;
  int ai = mrb_gc_arena_save(mrb);
  for (int i = MGEMS_POPULATION - 1; i > 0; i--, mgem--) {
    if (gemcut_loaded_p_by_id(gcut, i) && mgem->gem_final) {
      mrb_protect_error(mrb, gemcut_cleanup_main, (void *)(uintptr_t)mgem, NULL);
      mrb_gc_arena_restore(mrb, ai);
    }
  }
}

#define ID_GCARENA  mrb_intern_lit(mrb, "gcarena@mruby-gemcut")

static void
gemcut_snapshot_gc_arena(mrb_state *mrb)
{
  const mrb_gc *gc = &mrb->gc;
  mrb_value gcarena = mrb_ary_new_capa(mrb, gc->arena_idx);
  struct RBasic **bp = gc->arena;
  size_t i = gc->arena_idx;

  for (; i > 0; i--, bp++) {
    mrb_ary_push(mrb, gcarena, mrb_obj_value(*bp));
  }

  mrb_obj_freeze(mrb, gcarena);
  mrb_gv_set(mrb, ID_GCARENA, gcarena);
}

static void
gemcut_rollback_gc_arena_fallback(mrb_state *mrb, mrb_gc *gc, mrb_value gcarena)
{
  int i = gc->arena_capa;
  struct RBasic **bp = gc->arena;

  for (; i > 0; i--, bp++) {
    *bp = (struct RBasic *)mrb->object_class;
  }

  *gc->arena = (struct RBasic *)mrb_obj_ptr(gcarena);
  gc->arena_idx = 1;
}

static void
gemcut_rollback_gc_arena(mrb_state *mrb)
{
  mrb_gc *gc = &mrb->gc;
  mrb_value gcarena = mrb_gv_get(mrb, ID_GCARENA);
  mrb_gv_remove(mrb, ID_GCARENA);
  size_t arenalen = RARRAY_LEN(gcarena);

#ifdef MRB_GC_FIXED_ARENA
  if (arenalen > MRB_GC_ARENA_SIZE) {
    gemcut_rollback_gc_arena_fallback(mrb, gc, gcarena);
    return;
  }
#else
  if (arenalen > (size_t)gc->arena_capa) {
    mrb_value *p = (mrb_value *)mrb_realloc_simple(mrb, gc->arena, arenalen * sizeof(mrb_value));
    if (p == NULL) {
      gemcut_rollback_gc_arena_fallback(mrb, gc, gcarena);
      return;
    }
  }
#endif

  struct RBasic **bp = gc->arena;
  const mrb_value *vp = RARRAY_PTR(gcarena);
  size_t i = arenalen;

  for (; i > 0; i--, bp++, vp++) {
    if (mrb_immediate_p(*vp)) {
      *bp = (struct RBasic *)mrb->object_class;
    } else {
      *bp = (struct RBasic *)mrb_obj_ptr(*vp);
    }
  }

  gc->arena_idx = arenalen;
}

static mrb_value
gemcut_load_error(mrb_state *mrb, const char *name)
{
  mrb_value mesg = mrb_format(mrb, "cannot load such file - %" AUX_PRIs, AUX_PRIs_MAKE(name));
  mrb_value err = mrb_exc_new_str(mrb, mrb_exc_get(mrb, "LoadError"), mesg);
  if (mrb->jmp) {
    mrb_exc_raise(mrb, err);
  }
  return err;
}

struct gemcut_require_by_id_main_top
{
  struct gemcut *gcut;
  int id;
};

static void
gemcut_require_by_id_main(mrb_state *mrb, struct gemcut *gcut, int id, int ai)
{
  const struct mgem_spec *spec = &mgems_list[id];

  {
    const uint16_t *deps = spec->deps;
    for (int i = spec->numdeps; i > 0; i--, deps++) {
      if (!gemcut_loaded_p_by_id(gcut, *deps)) {
        gemcut_require_by_id_main(mrb, gcut, *deps, ai);
      }
    }
  }

  gemcut_set_loaded_by_id(gcut, id);
  if (spec->gem_init) {
    AUX_GEM_INIT_ENTER() {
      spec->gem_init(mrb);
      mrb_gc_arena_restore(mrb, ai);
    } AUX_GEM_INIT_LEAVE();
  }
}

static mrb_value
gemcut_require_by_id_main_top(mrb_state *mrb, void *opaque)
{
  struct gemcut_require_by_id_main_top *p = (struct gemcut_require_by_id_main_top *)opaque;
  if (!p->gcut->set_atexit) {
    mrb_state_atexit(mrb, gemcut_cleanup);
    p->gcut->set_atexit = true;
  }

  gemcut_require_by_id_main(mrb, p->gcut, p->id, mrb_gc_arena_save(mrb));

  return mrb_true_value();
}

static mrb_value
gemcut_require_main(mrb_state *mrb, void *opaque)
{
  struct gemcut *gcut = get_gemcut(mrb);

  if (gcut->status) {
    gemcut_sealed_error(mrb);
  }

  const char *name = (const char *)opaque;
  int id = gemcut_lookup(name);
  if (id < 0) {
    return gemcut_load_error(mrb, name);
  }

  if (gemcut_loaded_p_by_id(gcut, id)) {
    return mrb_false_value();
  }

  const struct mgem_spec *spec = &mgems_list[id];
  if (!spec->available) {
    return gemcut_load_error(mrb, spec->name);
  }

  struct gemcut_require_by_id_main_top args = { gcut, id };
  gemcut_snapshot_gc_arena(mrb);
  mrb_bool error;
  mrb_value ret = mrb_protect_error(mrb, gemcut_require_by_id_main_top, &args, &error);
  gemcut_rollback_gc_arena(mrb);

  if (error && mrb->jmp) {
    mrb_exc_raise(mrb, ret);
  }

  return ret;
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API mrb_value mruby_gemcut_require(mrb_state *mrb, const char *name),
    gemcut_require_main, (void *)(uintptr_t)name, RESULT_PASSTHROUGH, ret)

static mrb_value
gemcut_s_require(mrb_state *mrb, mrb_value mod)
{
  (void)mod;

  const char *name;
  mrb_get_args(mrb, "z", &name);
  return gemcut_require_main(mrb, (void *)(uintptr_t)name);
}

static mrb_value
gemcut_loaded_features_main(mrb_state *mrb, void *opaque)
{
  (void)opaque;

  struct gemcut *gcut = get_gemcut(mrb);
  mrb_value ary = mrb_ary_new(mrb);
  for (int i = 0; i < MGEMS_POPULATION; i++) {
    if (gemcut_loaded_p_by_id(gcut, i)) {
      mrb_ary_push(mrb, ary, mrb_str_new_static(mrb, mgems_list[i].name, strlen(mgems_list[i].name)));
    }
  }
  return ary;
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API mrb_value mruby_gemcut_loaded_features(mrb_state *mrb),
    gemcut_loaded_features_main, NULL, RESULT_PASSTHROUGH, mrb_nil_value())

static mrb_value
gemcut_s_loaded_features(mrb_state *mrb, mrb_value mod)
{
  (void)mod;

  gemcut_check_sealed(mrb);
  return gemcut_loaded_features_main(mrb, NULL);
}

static mrb_value
gemcut_loaded_count_main(mrb_state *mrb, void *opaque)
{
  struct gemcut *gcut = get_gemcut(mrb);
  int count = 0;
  for (int i = 0; i < MGEMS_BITMAP_UNITS; i++) {
    count += popcount32(gcut->loaded[i]);
  }

  if (opaque) {
    *(int *)opaque = count;
  }

  return mrb_fixnum_value(count);
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API int mruby_gemcut_loaded_count(mrb_state *mrb),
    gemcut_loaded_count_main, NULL, mrb_fixnum, -1)

static mrb_value
gemcut_s_loaded_feature_count(mrb_state *mrb, mrb_value mod)
{
  (void)mod;

  gemcut_check_sealed(mrb);
  return gemcut_loaded_count_main(mrb, NULL);
}

static mrb_value
gemcut_loaded_feature_p_main(mrb_state *mrb, void *opaque)
{
  struct gemcut *gcut = get_gemcut(mrb);
  int id = gemcut_lookup((const char *)opaque);
  return mrb_bool_value(gemcut_loaded_p_by_id(gcut, id));
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API mrb_bool mruby_gemcut_loaded_p(mrb_state *mrb, const char *name),
    gemcut_loaded_count_main, (void *)(uintptr_t)name, mrb_bool, FALSE)

static mrb_value
gemcut_s_loaded_feature_p(mrb_state *mrb, mrb_value mod)
{
  (void)mod;

  const char *name;
  mrb_get_args(mrb, "z", &name);
  gemcut_check_sealed(mrb);
  return gemcut_loaded_feature_p_main(mrb, (void *)(uintptr_t)name);
}

static mrb_value
gemcut_loadable_features_main(mrb_state *mrb, void *opaque)
{
  (void)opaque;
  (void)get_gemcut(mrb);

  mrb_value ary = mrb_ary_new(mrb);
  FOREACH_ALIST(const struct mgem_spec, *mgem, mgems_list) {
    if (mgem->available) {
      mrb_ary_push(mrb, ary, mrb_str_new_static(mrb, mgem->name, strlen(mgem->name)));
    }
  }

  return ary;
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API mrb_value mruby_gemcut_loadable_features(mrb_state *mrb),
    gemcut_loadable_features_main, NULL, RESULT_PASSTHROUGH, mrb_nil_value())

static mrb_value
gemcut_s_loadable_features(mrb_state *mrb, mrb_value mod)
{
  (void)mod;

  gemcut_check_sealed(mrb);
  return gemcut_loadable_features_main(mrb, NULL);
}

static mrb_value
gemcut_loadable_feature_count_main(mrb_state *mrb, void *opaque)
{
  (void)opaque;
  (void)get_gemcut(mrb);

  int count = 0;
  FOREACH_ALIST(const struct mgem_spec, *mgem, mgems_list) {
    count += mgem->available;
  }
  return mrb_fixnum_value(count);
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API int mruby_gemcut_loadable_count(mrb_state *mrb),
    gemcut_loadable_feature_count_main, NULL, mrb_fixnum, -1)

static mrb_value
gemcut_s_loadable_feature_count(mrb_state *mrb, mrb_value mod)
{
  (void)mod;

  gemcut_check_sealed(mrb);
  return gemcut_loadable_feature_count_main(mrb, NULL);
}

static mrb_value
gemcut_loadable_feature_p_main(mrb_state *mrb, void *opaque)
{
  (void)get_gemcut(mrb);

  const char *name = (const char *)opaque;
  int id = gemcut_lookup(name);
  if (id >= 0 && mgems_list[id].available) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API mrb_bool mruby_gemcut_loadable_p(mrb_state *mrb, const char *name),
    gemcut_loadable_feature_p_main, (void *)(uintptr_t)name, mrb_bool, FALSE)

static mrb_value
gemcut_s_loadable_feature_p(mrb_state *mrb, mrb_value mod)
{
  (void)mod;

  const char *name;
  mrb_get_args(mrb, "z", &name);
  gemcut_check_sealed(mrb);
  return gemcut_loadable_feature_p_main(mrb, (void *)(uintptr_t)name);
}

static mrb_value
gemcut_lock_main(mrb_state *mrb, void *opaque)
{
  (void)opaque;

  struct gemcut *g;
  get_gemcut_main(mrb, &g);
  if (!g->status) {
    g->status = gemcut_locked;
  }

  return mrb_nil_value();
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API void mruby_gemcut_lock(mrb_state *mrb),
    gemcut_lock_main, NULL, RESULT_VOID, RESULT_VOID_ERROR)

static mrb_value
gemcut_s_lock(mrb_state *mrb, mrb_value mod)
{
  (void)mod;

  return gemcut_lock_main(mrb, NULL);
}

static mrb_value
gemcut_seal_main(mrb_state *mrb, void *opaque)
{
  (void)opaque;

  struct gemcut *g;
  get_gemcut_main(mrb, &g);
  g->status = gemcut_sealed;
  mrb_const_remove(mrb, mrb_obj_value(mrb->object_class), mrb_intern_lit(mrb, "Gemcut"));

  return mrb_nil_value();
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API void mruby_gemcut_seal(mrb_state *mrb),
    gemcut_seal_main, NULL, RESULT_VOID, RESULT_VOID_ERROR)

static mrb_value
gemcut_s_seal(mrb_state *mrb, mrb_value mod)
{
  (void)mod;

  return gemcut_seal_main(mrb, NULL);
}

static mrb_value
gemcut_define_module(mrb_state *mrb, void *unused)
{
  (void)unused;

  struct gemcut *g = get_gemcut(mrb);
  if (!g->defined_module && g->status < gemcut_sealed) {
    g->defined_module = true;

    NO_PRESYM(mrb_intern_lit(mrb, "Gemcut"));
    struct RClass *gemcut_mod = mrb_define_module(mrb, "Gemcut");

    mrb_define_class_method(mrb, gemcut_mod, "require", gemcut_s_require, MRB_ARGS_REQ(1));

    mrb_define_class_method(mrb, gemcut_mod, "loaded_features", gemcut_s_loaded_features, MRB_ARGS_NONE());
    mrb_define_class_method(mrb, gemcut_mod, "loaded_feature_count", gemcut_s_loaded_feature_count, MRB_ARGS_NONE());
    mrb_define_class_method(mrb, gemcut_mod, "loaded_feature?", gemcut_s_loaded_feature_p, MRB_ARGS_REQ(1));

    mrb_define_class_method(mrb, gemcut_mod, "loadable_features", gemcut_s_loadable_features, MRB_ARGS_NONE());
    mrb_define_class_method(mrb, gemcut_mod, "loadable_feature_count", gemcut_s_loadable_feature_count, MRB_ARGS_NONE());
    mrb_define_class_method(mrb, gemcut_mod, "loadable_feature?", gemcut_s_loadable_feature_p, MRB_ARGS_REQ(1));

    mrb_define_class_method(mrb, gemcut_mod, "lock", gemcut_s_lock, MRB_ARGS_NONE());
    mrb_define_class_method(mrb, gemcut_mod, "lock!", gemcut_s_lock, MRB_ARGS_NONE());

    mrb_define_class_method(mrb, gemcut_mod, "seal", gemcut_s_seal, MRB_ARGS_NONE());
    mrb_define_class_method(mrb, gemcut_mod, "seal!", gemcut_s_seal, MRB_ARGS_NONE());
  }

  return mrb_nil_value();
}

DEFINE_PROTECTED_FUNCTION(
    MRB_API void mruby_gemcut_define_module(mrb_state *mrb),
    gemcut_define_module, NULL, RESULT_VOID, RESULT_VOID_ERROR)

void
mrb_mruby_gemcut_gem_init(mrb_state *mrb)
{
  gemcut_define_module(mrb, NULL);
  gemcut_set_loaded_by_id(get_gemcut(mrb), MRUBY_GEMCUT_ID);
}

void
mrb_mruby_gemcut_gem_final(mrb_state *mrb)
{
  (void)mrb;
}
