#ifndef MRUBY_GEMCUT_COMPAT_H
#define MRUBY_GEMCUT_COMPAT_H 1

#include <mruby.h>
#include <mruby/error.h> /* for mrb_protect_error() */
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

#if AUX_MRUBY_RELEASE_NO <= 30000
# if defined(MRB_NAN_BOXING) || defined(MRB_WORD_BOXING)
union gemcut_cptr_wrapper
{
  void *ptr;
  mrb_value val;
};

union gemcut_cptr_unwrapper
{
  mrb_value val;
  void *ptr;
};

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
  union gemcut_cptr_wrapper payload = { ptr };
  return payload.val;
}

static void *
aux_cptr(mrb_value val)
{
  union gemcut_cptr_unwrapper payload = { val };
  return payload.ptr;
}
# else
#  define aux_cptr_value mrb_cptr_value
#  define aux_cptr mrb_cptr
# endif // defined(MRB_NAN_BOXING) || defined(MRB_WORD_BOXING)

typedef mrb_value mrb_protect_error_f(mrb_state *mrb, void *opaque);

struct mrb_protect_error_wrap
{
  mrb_protect_error_f *body;
  void *opaque;
};

static mrb_value
mrb_protect_error_wrap(mrb_state *mrb, mrb_value val)
{
  const struct mrb_protect_error_wrap *wrap = (struct mrb_protect_error_wrap *)aux_cptr(val);
  return wrap->body(mrb, wrap->opaque);
}

static mrb_value
mrb_protect_error(mrb_state *mrb, mrb_protect_error_f *body, void *opaque, mrb_bool *error)
{
  struct mrb_protect_error_wrap wrap = { body, opaque };
  return mrb_protect(mrb, mrb_protect_error_wrap, aux_cptr_value(mrb, &wrap), error);
}
#endif // AUX_MRUBY_RELEASE_NO

#endif // MRUBY_GEMCUT_COMPAT_H
