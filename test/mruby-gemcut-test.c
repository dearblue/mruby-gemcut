#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <mruby-gemcut.h>
#include "../src/compat.h"
#include <string.h>

static mrb_protect_error_func sandbox_main;
struct sandbox_args
{
  //mrb_state *owner;
  mrb_bool load_gemcut;
  const char *code;
  mrb_int ngems;
  const mrb_value *gems;
};

static mrb_value
sandbox_main(mrb_state *mrb, void *opaque)
{
  struct sandbox_args *p = (struct sandbox_args *)opaque;

  if (p->load_gemcut) {
    mruby_gemcut_require(mrb, "mruby-gemcut");
  }

  for (int i = 0; i < (int)p->ngems; i++) {
    mrb_value e = p->gems[i];
    mrb_assert(mrb_string_p(e));
    mrb_assert(!memchr(RSTRING_PTR(e), '\0', RSTRING_LEN(e)));
    mruby_gemcut_require(mrb, RSTRING_PTR(e));
  }

  mrb_value ret = mrb_load_string(mrb, p->code);
  return mrb->exc ? mrb_obj_value(mrb->exc) : ret;
}

static mrb_value
sandbox_inspect(mrb_state *mrb, void *opaque)
{
  mrb_value *p = (mrb_value *)opaque;
  return mrb_inspect(mrb, *p);
}

static mrb_value
gemcut_load_string(mrb_state *mrb, mrb_value self)
{
  struct sandbox_args args;
  //args.owner = mrb;
  mrb_get_args(mrb, "bz*", &args.load_gemcut, &args.code, &args.gems, &args.ngems);

  mrb_state *sandbox = mrb_open_core(mrb_default_allocf, NULL);
  mrb_bool err;
  mrb_value sandbox_ret = mrb_protect_error(sandbox, sandbox_main, &args, &err);
  sandbox_ret = mrb_protect_error(sandbox, sandbox_inspect, &sandbox_ret, &err);
  mrb_value master_ret; // TODO: master_ret への変換時に例外が発生した時、sandbox が迷子にならないようにする
  if (err) {
    master_ret = mrb_str_new_lit(mrb, "**RESULT IS AN ERROR**");
  } else if (!mrb_string_p(sandbox_ret)) {
    master_ret = mrb_str_new_lit(mrb, "**RESULT IS NOT A STRING**");
  } else {
    master_ret = mrb_str_new(mrb, RSTRING_PTR(sandbox_ret), RSTRING_LEN(sandbox_ret));
  }
  mrb_close(sandbox);

  return master_ret;
}

void
mrb_mruby_gemcut_gem_test(mrb_state *mrb)
{
  mrb_define_method(mrb, mrb->object_class, "gemcut_load_string", gemcut_load_string, MRB_ARGS_ANY());
}
