#include <mruby-gemcut.h>
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <stdarg.h>

static void
load_string(mrb_bool need_module, const char ruby[], int numgemcut, ...)
{
  mrb_state *mrb = mrb_open_core(mrb_default_allocf, NULL);

  if (numgemcut) {
    va_list va;
    va_start(va, numgemcut);
    for (; numgemcut > 0; numgemcut--) {
      mruby_gemcut_require(mrb, va_arg(va, const char *));
    }
    va_end(va);
  }

  if (need_module) {
    mruby_gemcut_require(mrb, "mruby-gemcut");
  }
  mrb_value gems = mruby_gemcut_loaded_features(mrb);
  if (!mrb_nil_p(gems)) {
    mrb_funcall_argv(mrb, gems, mrb_intern_cstr(mrb, "sort!"), 0, NULL);
  }
  printf(">> loaded gems: %s\n", mrb_str_to_cstr(mrb, mrb_inspect(mrb, gems)));
  fflush(stdout);

  mrb_value ret = mrb_load_string(mrb, ruby);
  if (mrb->exc) { ret = mrb_obj_value(mrb->exc); }
  if (mrb_exception_p(ret)) {
    ret = mrb_inspect(mrb, ret);
    fprintf(stderr, "raised exceptions - %s\n", mrb_string_value_cstr(mrb, &ret));
    fflush(stderr);
  }

  mrb_close(mrb);
}

int
main(int argc, char *argv[])
{
  load_string(FALSE, "puts 'a'", 0);
  load_string(FALSE, "puts 'b'", 1, "mruby-print");
  load_string(FALSE, "puts '%s' % 'c'", 1, "mruby-print");
  load_string(FALSE, "puts '%s' % 'd'", 2, "mruby-sprintf", "mruby-print");
  load_string(FALSE, "Math.sin 5", 2, "mruby-sprintf", "mruby-print");
  load_string(FALSE, "p Math.sin 5", 1, "mruby-math");
  load_string(FALSE, "p Math.sin 5", 2, "mruby-math", "mruby-print");

  load_string(TRUE, "p Gemcut.loaded_features.sort", 1, "mruby-print");
  load_string(TRUE, "p Gemcut.loaded_features.sort", 3, "mruby-sprintf", "mruby-math", "mruby-print");
  load_string(TRUE, "p 'mruby-math' => Gemcut.loaded_feature?('mruby-math')", 2, "mruby-math", "mruby-print");
  load_string(TRUE, "Gemcut.require 'mruby-sprintf'; p Gemcut.loaded_features.sort", 2, "mruby-math", "mruby-print");
  load_string(TRUE, "p 'mruby-string-ext' => Gemcut.loaded_feature?('mruby-string-ext')", 3, "mruby-sprintf", "mruby-math", "mruby-print");

  load_string(FALSE, "Gemcut.require 'mruby-gemcut'; Gemcut.require 'mruby-print'; p Gemcut.loaded_features.sort", 0);
  load_string(TRUE, "Gemcut.require 'mruby-gemcut'; Gemcut.require 'mruby-print'; p Gemcut.loaded_features.sort", 0);
  load_string(TRUE, "Gemcut.require 'mruby-hash-ext'; Gemcut.require 'mruby-print'; p Gemcut.loaded_features.sort", 0);

  return 0;
}
