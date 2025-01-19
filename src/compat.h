#ifndef MRUBY_GEMCUT_COMPAT_H
#define MRUBY_GEMCUT_COMPAT_H 1

#include <mruby.h>
#include <mruby/error.h> /* for mrb_protect_error() */
#include <mruby/proc.h>
#include <mruby/version.h>

#if MRUBY_RELEASE_NO == 30000 && defined(mrb_as_int)
# define AUX_MRUBY_RELEASE_NO 30001L
#else
# define AUX_MRUBY_RELEASE_NO (MRUBY_RELEASE_NO)
#endif

#if AUX_MRUBY_RELEASE_NO < 20100
static void
mrb_obj_freeze(mrb_state *mrb, mrb_value obj)
{
  (void)mrb;
# if AUX_MRUBY_RELEASE_NO < 10300
  (void)obj;
# else
  if (!mrb_immediate_p(obj)) {
    MRB_SET_FROZEN_FLAG(mrb_obj_ptr(obj));
  }
# endif
}
#endif

#if AUX_MRUBY_RELEASE_NO >= 20100
# define AUX_PRIs "s"
# define AUX_PRIs_MAKE(STR) (STR)
#else
# define AUX_PRIs "S"
# define AUX_PRIs_MAKE(STR) mrb_str_new_cstr(mrb, (STR))
#endif

#if AUX_MRUBY_RELEASE_NO <= 30000
# if defined(MRB_NAN_BOXING) || defined(MRB_WORD_BOXING)
#  if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) || \
      (defined(__cplusplus) && __cplusplus >= 201103L)
#  include <assert.h>
static_assert(sizeof(mrb_value) >= sizeof(void *), "need MRB_NO_BOXING");
#  endif

/*
 * mrb_cptr_value() は NoMemoryError 例外を起こす可能性があるためすり替える
 */
static mrb_value
aux_cptr_value(mrb_state *mrb, void *ptr)
{
  (void)mrb;

  union { void *ptr; mrb_value val; } payload = { ptr };
  return payload.val;
}

static void *
aux_cptr(mrb_value val)
{
  union { mrb_value val; void *ptr; } payload = { val };
  return payload.ptr;
}
# else
#  define aux_cptr_value mrb_cptr_value
#  define aux_cptr mrb_cptr
# endif // defined(MRB_NAN_BOXING) || defined(MRB_WORD_BOXING)

typedef mrb_value mrb_protect_error_func(mrb_state *mrb, void *opaque);

struct mrb_protect_error_wrap
{
  mrb_protect_error_func *body;
  void *opaque;
};

static mrb_value
mrb_protect_error_wrap(mrb_state *mrb, mrb_value val)
{
  const struct mrb_protect_error_wrap *wrap = (struct mrb_protect_error_wrap *)aux_cptr(val);
  return wrap->body(mrb, wrap->opaque);
}

static mrb_value
mrb_protect_error(mrb_state *mrb, mrb_protect_error_func *body, void *opaque, mrb_bool *error)
{
  struct mrb_protect_error_wrap wrap = { body, opaque };
  return mrb_protect(mrb, mrb_protect_error_wrap, aux_cptr_value(mrb, &wrap), error);
}
#endif // AUX_MRUBY_RELEASE_NO <= 30000

#if AUX_MRUBY_RELEASE_NO <= 10200
static struct RClass *
mrb_exc_get(mrb_state *mrb, const char name[])
{
  mrb_value v = mrb_const_get(mrb, mrb_obj_value(mrb->object_class), mrb_intern_cstr(mrb, name));
  mrb_check_type(mrb, v, MRB_TT_CLASS);

  struct RClass *c = mrb_class_ptr(v);
  if (MRB_INSTANCE_TT(c) == MRB_TT_EXCEPTION) {
    return c;
  } else {
    return mrb->eException_class;
  }
}
#endif

#ifndef mrb_proc_p // mruby-2.1.0 で登場
# define mrb_proc_p(V)          (!mrb_immediate_p(V) && mrb_basic_ptr(V)->tt == MRB_TT_PROC)
#endif

#if MRUBY_RELEASE_NO >= 30000
# define CI_STACK(FC)           ((FC)->ci->stack)
#else
# define CI_STACK(FC)           ((FC)->stack)
#endif

#if MRUBY_RELEASE_NO >= 30100
# define CI_FLAT_ARGC(FC)       ((FC)->ci->n)
#else
# define CI_FLAT_ARGC(FC)       ((FC)->ci->argc)
#endif

#ifndef ARY_EMBED_P // mruby-1.4.0 で登場
# define ARY_EMBED_P(A)         (FALSE)
#endif

#ifndef ARY_SET_LEN
# define ARY_SET_LEN(A, N)      do { (A)->len = (N); } while (0)
#endif

#ifndef mrb_exc_new_lit // mruby-3.0.0 で登場
# define mrb_exc_new_lit(M, C, S) mrb_exc_new_str_lit(M, C, S)
#endif

#define MRB_RAISE_LIT(MRB, C, STRLIT) mrb_exc_raise(MRB, mrb_exc_new_lit(MRB, C, "" STRLIT))

#ifndef RSTRING_CSTR // mruby-2.1.0 で登場
static inline const char *
mrb_string_cstr(mrb_state *mrb, mrb_value obj)
{
  return mrb_string_value_cstr(mrb, &obj);
}
#endif

#ifndef mrb_alignas
# if defined(__cplusplus) && __cplusplus >= 201103L
#  // https://ja.cppreference.com/w/cpp/language/alignas
#  define mrb_alignas(n) alignas(n)
# elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  // https://ja.cppreference.com/w/c/language/_Alignas
#  define mrb_alignas(n) _Alignas(n)
# elif defined(__GNUC__) || defined(__clang__)
#  // https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#index-aligned-type-attribute
#  define mrb_alignas(n) __attribute__((aligned(n)))
# elif defined(_MSC_VER)
#  // https://learn.microsoft.com/en-us/cpp/cpp/align-cpp?view=msvc-170
#  define mrb_alignas(n) __declspec(align(n))
# else
#  define mrb_alignas(n)
# endif
#endif

#endif // MRUBY_GEMCUT_COMPAT_H
