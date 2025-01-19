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
  const struct gemcut_model *model;
};

static mrb_value gemcut_require_by_id(mrb_state *mrb, struct gemcut *gcut, int id);

static mrb_bool
gemcut_is_any_loaded(const struct gemcut *g)
{
  bitmap_unit m = 0;
  for (int i = 0; i < MGEMS_BITMAP_UNITS; i++) {
    m |= g->loaded[i];
  }

  return !!m;
}

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

#define IS_MATCH_NAME(N, NLEN, M, MLEN) ((NLEN) == (MLEN) && memcmp(N, M, NLEN) == 0)
#define IS_MATCH_ENTRY_NAME(N, NLEN, E) IS_MATCH_NAME(N, NLEN, gemcut_name_table + (E)[-1].name_index_end, (E)[-1].name_index_end - (E)[0].name_index_end)

static int
gemcut_lookup(mrb_state *mrb, const char name[], mrb_bool autoprefix)
{
  size_t namelen = strlen(name);

  for (;;) {
    FOREACH_ALIST(const struct mrbgem_spec, *mgem, mrbgems_list) {
      size_t name_beg = (mgem == mrbgems_list) ? 0 : mgem[-1].name_index_end;
      if (IS_MATCH_NAME(name, namelen, gemcut_name_table + name_beg, mgem->name_index_end - name_beg)) {
        return mgem - mrbgems_list;
      }
    }

    if (!autoprefix) {
      break;
    } else {
      autoprefix = FALSE;
      int ai = mrb_gc_arena_save(mrb);
      mrb_value str = mrb_str_new_cstr(mrb, "mruby-");
      mrb_str_cat_cstr(mrb, str, name);
      name = mrb_str_to_cstr(mrb, str);
      namelen = RSTRING_LEN(str);
      mrb_gc_arena_restore(mrb, ai);
    }
  }

  return -1;
}

static mrb_bool
model_is_available(const struct gemcut_model *model, size_t id)
{
  return (id < MGEMS_POPULATION) &&
         (model->avail[id / MGEMS_UNIT_BITS] & (1UL << (id % MGEMS_UNIT_BITS)));
}

static mrb_bool
model_is_available_by_gem(const struct gemcut_model *model, const struct mrbgem_spec *gem)
{
  return model_is_available(model, (size_t)(gem - mrbgems_list));
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
    (*gcutp)->model = &gemcut_models[0];
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
  int ai = mrb_gc_arena_save(mrb);
  struct gemcut *gcut;
  mrb_bool state;
  mrb_protect_error(mrb, (mrb_value (*)(mrb_state *, void *))get_gemcut_main, &gcut, &state);
  mrb_gc_arena_restore(mrb, ai);

  if (state) {
    return NULL;
  } else {
    return gcut;
  }
}

static int
gemcut_model_require_bundles(mrb_state *mrb, struct gemcut *gcut)
{
  for (int i = 0; i < MGEMS_POPULATION; i++) {
    if (gcut->model->bundle[i / MGEMS_UNIT_BITS] & (1UL << (i % MGEMS_UNIT_BITS))) {
      mrb_value ret = gemcut_require_by_id(mrb, gcut, i);
      if (mrb_exception_p(ret)) {
        return 1;
      }
    }
  }

  return 0;
}

MRB_API int
mruby_gemcut_model_select(mrb_state *mrb, const char model_name[])
{
  struct gemcut *gcut = get_gemcut_noraise(mrb);
  if (gcut == NULL || gemcut_is_any_loaded(gcut)) { return 1; }

  gcut->model = &gemcut_models[0];
  if (model_name) {
    size_t len = strlen(model_name);

    FOREACH_ALIST(const struct gemcut_model, *p, gemcut_models) {
      if (p != gemcut_models && IS_MATCH_ENTRY_NAME(model_name, len, p)) {
        gcut->model = p;
        break;
      }
    }
  }

  return gemcut_model_require_bundles(mrb, gcut);
}

MRB_API const char *
mruby_gemcut_model_name(mrb_state *mrb)
{
  struct gemcut *gcut = get_gemcut_noraise(mrb);
  if (gcut == NULL || gcut->model == &gemcut_models[0]) {
    return NULL;
  } else {
    return gemcut_name_table + gcut->model[-1].name_index_end;
  }
}

MRB_API mrb_value
mruby_gemcut_model_list(mrb_state *mrb)
{
  mrb_value list = mrb_ary_new_capa(mrb, sizeof(gemcut_models) / sizeof(gemcut_models[0]) - 1);
  FOREACH_ALIST(const struct gemcut_model, *p, gemcut_models) {
    if (p != &gemcut_models[0]) {
      size_t name_beg = p[-1].name_index_end;
      mrb_ary_push(mrb, list, mrb_str_new_static(mrb, gemcut_name_table + name_beg, p->name_index_end - name_beg));
    }
  }
  return list;
}

MRB_API size_t
mruby_gemcut_model_size(mrb_state *mrb)
{
  (void)mrb;

  return sizeof(gemcut_models) / sizeof(gemcut_models[0]) - 1 /* reject the default */;
}

MRB_API mrb_bool
mruby_gemcut_model_p(mrb_state *mrb, const char model_name[])
{
  (void)mrb;

  if (model_name) {
    size_t len = strlen(model_name);

    FOREACH_ALIST(const struct gemcut_model, *p, gemcut_models) {
      if (p != &gemcut_models[0] && IS_MATCH_ENTRY_NAME(model_name, len, p)) {
        return TRUE;
      }
    }
  }

  return FALSE;
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
      gemcut_require_by_id(dest, gdest, i);
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
  const struct mrbgem_spec *mgem = (const struct mrbgem_spec *)opaque;
  mgem->gem_final(mrb);
  return mrb_nil_value();
}

static void
gemcut_cleanup(mrb_state *mrb)
{
  struct gemcut *gcut = get_gemcut_noraise(mrb);
  if (gcut == NULL) { return; }

  const struct mrbgem_spec *mgem = mrbgems_list + MGEMS_POPULATION - 1;
  int ai = mrb_gc_arena_save(mrb);
  for (int i = MGEMS_POPULATION - 1; i >= 0; i--, mgem--) {
    if (gemcut_loaded_p_by_id(gcut, i) && mgem->gem_final) {
      mrb_protect_error(mrb, gemcut_cleanup_main, (void *)(uintptr_t)mgem, NULL);
      mrb_gc_arena_restore(mrb, ai);
    }
  }
}

#ifdef MRB_GC_RED // MRUBY_RELEASE_NO >= 30100
# ifdef MRB_OBJ_IS_FROZEN
#  define MAKE_OBJ_FLAG_BLOCK(FROZEN, REST) ((FROZEN) ? MRB_OBJ_IS_FROZEN : 0), (REST)
# else
#  define MAKE_OBJ_FLAG_BLOCK(FROZEN, REST) (((FROZEN) ? MRB_FL_OBJ_IS_FROZEN : 0) | (REST))
# endif
# define MAKE_FUNC_AGET_PROC_FROM_CFUNC(NAME, FUNC)                           \
  static mrb_value                                                            \
  NAME(mrb_state *mrb)                                                        \
  {                                                                           \
    mrb_alignas(8)                                                            \
    static const struct RProc proc = {                                        \
      NULL, NULL, MRB_TT_PROC, MRB_GC_RED, MAKE_OBJ_FLAG_BLOCK(TRUE, MRB_PROC_CFUNC_FL), \
      { (const mrb_irep *)FUNC }, NULL, { NULL }                              \
    };                                                                        \
    return mrb_obj_value((void *)&proc);                                      \
  }                                                                           \

#else
# define MAKE_FUNC_AGET_PROC_FROM_CFUNC(NAME, FUNC)                           \
  static mrb_value                                                            \
  NAME(mrb_state *mrb)                                                        \
  {                                                                           \
    mrb_value val = mrb_gv_get(mrb, mrb_intern_lit(mrb, #FUNC ":mruby-gemcut")); \
    if (mrb_proc_p(val)) {                                                    \
      struct RProc *p = mrb_proc_ptr(val);                                    \
                                                                              \
      if (MRB_PROC_CFUNC_P(p) && p->body.func == FUNC) {                      \
        return val;                                                           \
      }                                                                       \
    }                                                                         \
                                                                              \
    int ai = mrb_gc_arena_save(mrb);                                          \
    struct RProc *p = mrb_proc_new_cfunc(mrb, FUNC);                          \
    mrb_gv_set(mrb, mrb_intern_lit(mrb, #FUNC ":mruby-gemcut"), mrb_obj_value(p)); \
    mrb_gc_arena_restore(mrb, ai);                                            \
    p->c = NULL;                                                              \
                                                                              \
    return mrb_obj_value(p);                                                  \
  }                                                                           \

#endif

static mrb_value
gemcut_snapshot_gc_arena(mrb_state *mrb)
{
  const mrb_gc *gc = &mrb->gc;
  int ai = gc->arena_idx;
  mrb_value arena = mrb_ary_new_capa(mrb, ai);

  struct RBasic **bp = gc->arena;
  mrb_value *vect = (mrb_value *)RARRAY_PTR(arena);
  for (int i = ai; i > 0; i--) {
    *vect++ = mrb_obj_value(*bp++);
  }
  ARY_SET_LEN(mrb_ary_ptr(arena), ai);
  mrb_write_barrier(mrb, mrb_basic_ptr(arena));
  mrb_obj_freeze(mrb, arena);
  mrb_basic_ptr(arena)->c = NULL;

  return arena;
}

#ifdef MRB_GC_FIXED_ARENA
# define MRB_GC_ARENA_CAPA(GC) MRB_GC_ARENA_SIZE
#else
# define MRB_GC_ARENA_CAPA(GC) ((GC)->arena_capa)
#endif

#ifndef MRB_GC_FIXED_ARENA
static void
upgrade_arena(mrb_state *mrb, mrb_gc *gc, size_t estimate)
{
  mrb_assert(gc->arena_capa < MRB_GC_ARENA_SIZE);
  size_t capa = MRB_GC_ARENA_SIZE;
  while (capa < estimate) {
    capa += capa >> 1;
  }
  gc->arena = (struct RBasic **)mrb_realloc(mrb, gc->arena, capa * sizeof(struct RBasic *));
  gc->arena_capa = capa;
}
#endif

static void
gemcut_rollback_gc_arena(mrb_state *mrb, mrb_value arena)
{
  mrb_gc *gc = &mrb->gc;

  if (RARRAY_LEN(arena) > MRB_GC_ARENA_CAPA(gc)) {
#ifdef MRB_GC_FIXED_ARENA
    MRB_RAISE_LIT(mrb, mrb->eException_class,
                  "[CRITICAL ERROR]"
                  " IN " __FILE__ ":" MRB_STRINGIZE(__LINE__) ","
                  " FOR UNKNOWN REASONS, THE NUMBER OF PROTECTED ARENA OBJECTS EXCEEDED THE ORIGINAL GC ARENA."
                  " A SUDDEN APPLICATION CRASH MAY OCCUR IF PROCESSING CONTINUES."
                  "[CRITICAL ERROR]");
#else
    if (ARY_EMBED_P(mrb_ary_ptr(arena)) || MRB_GC_ARENA_CAPA(gc) < MRB_GC_ARENA_SIZE) {
      upgrade_arena(mrb, gc, RARRAY_LEN(arena));
    } else {
      const mrb_value *ap = RARRAY_PTR(arena);
      struct RBasic **bp = (struct RBasic **)ap;
      int i = RARRAY_LEN(arena);
      ap += i;
      bp += i;
      for (; i > 0; i--) {
        mrb_assert(!mrb_immediate_p(*ap));
        *++bp = mrb_basic_ptr(*++ap);
      }
      mrb_free(mrb, gc->arena);
      gc->arena = bp;
# if MRUBY_RELEASE_NO >= 10400
      mrb_ary_ptr(arena)->as.heap.ptr = NULL;
      ARY_SET_LEN(mrb_ary_ptr(arena), 0);
      mrb_ary_ptr(arena)->as.heap.aux.capa = 0;
# else
      mrb_ary_ptr(arena)->ptr = NULL;
      mrb_ary_ptr(arena)->len = 0;
      mrb_ary_ptr(arena)->aux.capa = 0;
# endif
    }
#endif
  }

  struct RBasic **bp = gc->arena;
  const mrb_value *ap = RARRAY_PTR(arena);
  gc->arena_idx = RARRAY_LEN(arena);
  for (int i = gc->arena_idx; i > 0; i--) {
    mrb_assert(!mrb_immediate_p(*ap));
    *bp++ = mrb_basic_ptr(*ap++);
  }
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

struct gemcut_require_by_id_main
{
  struct gemcut *gcut;
  int id;
  int ai;
};

static void
gemcut_require_by_id_main(mrb_state *mrb, struct gemcut *gcut, int id, int *ai)
{
  const struct mrbgem_spec *spec = &mrbgems_list[id];

  if (id == 0) {
    mrb_assert(spec[0].deps_index_end == 0);
  } else {
    const gemcut_deps_index_t *deps = mrbgems_deps_list + spec[-1].deps_index_end;
    gemcut_deps_index_t deps_num = spec[0].deps_index_end - spec[-1].deps_index_end;
    for (int i = deps_num; i > 0; i--, deps++) {
      if (!gemcut_loaded_p_by_id(gcut, *deps)) {
        gemcut_require_by_id_main(mrb, gcut, *deps, ai);
      }
    }
  }

  gemcut_set_loaded_by_id(gcut, id);
  if (spec->gem_init) {
#if AUX_MRUBY_RELEASE_NO >= 30100
    spec->gem_init(mrb);
#else
    int cioff = mrb->c->ci - mrb->c->cibase;
    spec->gem_init(mrb);
    mrb->c->ci = mrb->c->cibase + cioff;
#endif

    int ai1 = mrb_gc_arena_save(mrb);
    if (ai1 < *ai) {
      *ai = ai1;
    } else {
      mrb_gc_arena_restore(mrb, *ai);
    }
  }
}

static mrb_value
gemcut_require_by_id_guard3(mrb_state *mrb, mrb_value self)
{
  const mrb_value *argv = CI_STACK(mrb->c) + 1;
  if (CI_FLAT_ARGC(mrb->c) != 1 || !mrb_cptr_p(argv[0])) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "broken assumptions");
  }

  struct gemcut_require_by_id_main *p = (struct gemcut_require_by_id_main *)mrb_cptr(argv[0]);

  int ai = p->ai;
  gemcut_require_by_id_main(mrb, p->gcut, p->id, &ai);

  return mrb_true_value();
}

MAKE_FUNC_AGET_PROC_FROM_CFUNC(gemcut_require_by_id_guard3_proc, gemcut_require_by_id_guard3)

static mrb_value
gemcut_require_by_id_guard2(mrb_state *mrb, void *opaque)
{
  mrb_value *p = (mrb_value *)opaque;

  return mrb_yield_with_class(mrb, gemcut_require_by_id_guard3_proc(mrb), 1, p, mrb_top_self(mrb), mrb->object_class);
}

static mrb_value
gemcut_require_by_id_guard1(mrb_state *mrb, mrb_value self)
{
  const mrb_value *argv = CI_STACK(mrb->c) + 1;
  if (CI_FLAT_ARGC(mrb->c) != 2 || !mrb_cptr_p(argv[0]) || !mrb_array_p(argv[1])) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "broken assumptions");
  }

  struct gemcut_require_by_id_main *p = (struct gemcut_require_by_id_main *)mrb_cptr(argv[0]);
  mrb_gc_arena_restore(mrb, p->ai);

  mrb_bool error;
  mrb_value ret = mrb_protect_error(mrb, gemcut_require_by_id_guard2, (void *)&argv[0], &error);
  gemcut_rollback_gc_arena(mrb, argv[1]);

  if (error && mrb->jmp) {
    mrb_exc_raise(mrb, ret);
  }

  return ret;
}

MAKE_FUNC_AGET_PROC_FROM_CFUNC(gemcut_require_by_id_guard1_proc, gemcut_require_by_id_guard1)

static mrb_value
gemcut_require_by_id(mrb_state *mrb, struct gemcut *gcut, int id)
{
  //  考慮するべきこと:
  //    ・mrb_gc_arena_restore(mrb, 0) の形でアリーナからの束縛をすべて解放してしまう GEM がある。
  //    ・GENERATED_TMP_mrb_***_gem_init() から呼び出される mrb_top_run() は、現在のデータスタックを破壊する。
  //    ・呼び出し API が C なのか Ruby なのか区別していないため、現在のデータスタックを保護する必要がある。
  //
  //  GENERATED_TMP_mrb_***_gem_init()
  //  mrb_yield_with_class()            // mrb_top_run() からデータスタックを保護する
  //  mrb_protect_error()               // ensure 相当が目的で、GC アリーナを復帰する
  //  mrb_yield_with_class()            // 目的は GC アリーナのミラー配列を保護する
  //  gemcut_require_by_id()            // イマココ！

  if (!gcut->set_atexit) {
    mrb_state_atexit(mrb, gemcut_cleanup);
    gcut->set_atexit = true;
  }

  TODO("mrb_yield_with_class() を経由する MRB_TT_CPTR を避ける (MRB_TT_CDATA に置き換える)")

  struct gemcut_require_by_id_main args = { gcut, id, mrb_gc_arena_save(mrb) };
  struct { mrb_value args, arena; } argv = { mrb_cptr_value(mrb, &args), gemcut_snapshot_gc_arena(mrb) };
  mrb_value ret = mrb_yield_with_class(mrb, gemcut_require_by_id_guard1_proc(mrb), 2, &argv.args, mrb_top_self(mrb), mrb->object_class);

  mrb_gc_arena_restore(mrb, args.ai);

  return ret;
}

static mrb_value
gemcut_require_main(mrb_state *mrb, void *opaque)
{
  struct gemcut *gcut = get_gemcut(mrb);

  if (gcut->status) {
    gemcut_sealed_error(mrb);
  }

  const char *name = (const char *)opaque;
  int id = gemcut_lookup(mrb, name, TRUE);
  if (id < 0) {
    return gemcut_load_error(mrb, name);
  }

  if (gemcut_loaded_p_by_id(gcut, id)) {
    return mrb_false_value();
  }

  if (!model_is_available(gcut->model, id)) {
    size_t name_beg = (id == 0) ? 0 : mrbgems_list[id - 1].name_index_end;
    return gemcut_load_error(mrb, gemcut_name_table + name_beg);
  }

  return gemcut_require_by_id(mrb, gcut, id);
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

/*
 *  call-seq:
 *      facet(*features)   ->   nil
 *
 *  複数の GEM 名を指定できる `require` メソッドです。
 *  常に `nil` を返します。
 */
static mrb_value
gemcut_s_facet(mrb_state *mrb, mrb_value mod)
{
  (void)mod;

  const mrb_value *argv;
  mrb_int argc;
  mrb_get_args(mrb, "*!", &argv, &argc);

  int ai = mrb_gc_arena_save(mrb);
  for (int i = 0; i < (int)argc; i++) {
    gemcut_require_main(mrb, (void *)(uintptr_t)mrb_string_cstr(mrb, argv[i]));
    mrb_gc_arena_restore(mrb, ai);
  }

  return mrb_nil_value();
}

static mrb_value
gemcut_loaded_features_main(mrb_state *mrb, void *opaque)
{
  (void)opaque;

  struct gemcut *gcut = get_gemcut(mrb);
  mrb_value ary = mrb_ary_new(mrb);
  for (int i = 0; i < MGEMS_POPULATION; i++) {
    if (gemcut_loaded_p_by_id(gcut, i)) {
      const struct mrbgem_spec *g = &mrbgems_list[i];
      size_t name_beg = (i == 0) ? 0 : g[-1].name_index_end;
      mrb_ary_push(mrb, ary, mrb_str_new_static(mrb, gemcut_name_table + name_beg, g[0].name_index_end - name_beg));
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
  int id = gemcut_lookup(mrb, (const char *)opaque, TRUE);
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

  const struct gemcut_model *model = get_gemcut(mrb)->model;

  mrb_value ary = mrb_ary_new(mrb);
  FOREACH_ALIST(const struct mrbgem_spec, *mgem, mrbgems_list) {
    if (model_is_available_by_gem(model, mgem)) {
      size_t name_beg = (mgem == mrbgems_list) ? 0 : mgem[-1].name_index_end;
      mrb_ary_push(mrb, ary, mrb_str_new_static(mrb, gemcut_name_table + name_beg, mgem[0].name_index_end - name_beg));
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

  const struct gemcut_model *model = get_gemcut(mrb)->model;

  int count = 0;
  FOREACH_ALIST(const struct mrbgem_spec, *mgem, mrbgems_list) {
    if (model_is_available_by_gem(model, mgem)) {
      count++;
    }
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
  struct gemcut *g = get_gemcut(mrb);

  const char *name = (const char *)opaque;
  int id = gemcut_lookup(mrb, name, TRUE);
  if (model_is_available(g->model, id)) {
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

#if MRUBY_RELEASE_NO >= 30300 && defined(MRUBY_GEMCUT_NEED_PRINT) && !defined(MRB_NO_STDIO)
static int
aux_get_argc(mrb_state *mrb)
{
  mrb_int argc = mrb_get_argc(mrb);

# if MRB_INT_MAX > INT_MAX
  if (argc > INT_MAX) {
    mrb_raise(mrb, E_RANGE_ERROR, "number too big");
  }
# endif

  (void)mrb;
  return (int)argc;
}

static mrb_value
kernel_print(mrb_state *mrb, mrb_value self)
{
  int ai = mrb_gc_arena_save(mrb);
  int argc = aux_get_argc(mrb);

  for (int i = 0; i < argc; i++, mrb_gc_arena_restore(mrb, ai)) {
    mrb_value str = mrb_get_argv(mrb)[i];

    if (!mrb_string_p(str)) {
      str = mrb_obj_as_string(mrb, str);
    }

    size_t len = RSTRING_LEN(str);
    if (fwrite(RSTRING_PTR(str), sizeof(char), len, stdout) != len) {
      mrb_sys_fail(mrb, "failed write to stdout");
    }
  }

  return mrb_nil_value();
}
#else
# undef MRUBY_GEMCUT_NEED_PRINT
#endif

void
mrb_mruby_gemcut_gem_init(mrb_state *mrb)
{
  NO_PRESYM(mrb_intern_lit(mrb, "Gemcut"));
  struct RClass *gemcut_mod = mrb_define_module(mrb, "Gemcut");

  mrb_define_class_method(mrb, gemcut_mod, "require", gemcut_s_require, MRB_ARGS_REQ(1));
  mrb_define_class_method(mrb, gemcut_mod, "facet", gemcut_s_facet, MRB_ARGS_ANY());

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

#ifdef MRUBY_GEMCUT_NEED_PRINT
  mrb_define_method(mrb, mrb->kernel_module, "print", kernel_print, MRB_ARGS_ANY());
#endif
}

void
mrb_mruby_gemcut_gem_final(mrb_state *mrb)
{
  (void)mrb;
}
