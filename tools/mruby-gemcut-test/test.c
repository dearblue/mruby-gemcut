#include <mruby-gemcut.h>
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <stdarg.h>

static mrb_value
load_string_trial(mrb_state *mrb, mrb_value obj)
{
  const char *ruby = (const char *)mrb_cptr(obj);
  return mrb_load_string(mrb, ruby);
}

static void
load_string(mrb_bool need_module, const char ruby[], int numgemcut, ...)
{
  mrb_state *mrb = mrb_open_core(mrb_default_allocf, NULL);

  if (numgemcut) {
    va_list va;
    va_start(va, numgemcut);
    for (; numgemcut > 0; numgemcut --) {
      mruby_gemcut_pickup(mrb, va_arg(va, const char *));
    }
    va_end(va);
  }

  if (need_module) {
    mruby_gemcut_define_module(mrb);
  } else {
    mruby_gemcut_commit(mrb);
  }
  printf(">> committed gems: %s\n", mrb_str_to_cstr(mrb, mrb_inspect(mrb, mruby_gemcut_committed_list(mrb))));
  fflush(stdout);

  mrb_value ret = load_string_trial(mrb, mrb_cptr_value(mrb, (void *)(uintptr_t)ruby));
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

  load_string(TRUE, "puts 'pickup required!'", 0);
  load_string(TRUE, "puts 'commit required!'", 1, "mruby-print");
  load_string(TRUE, "GemCut.commit", 1, "mruby-print");
  load_string(TRUE, "GemCut.commit; p GemCut.committed_list", 1, "mruby-print");
  load_string(TRUE, "GemCut.commit; puts '%p' % GemCut.committed_list", 3, "mruby-sprintf", "mruby-math", "mruby-print");
  load_string(TRUE, "GemCut.commit; p 'mruby-math' => GemCut.committed?('mruby-math')", 3, "mruby-sprintf", "mruby-math", "mruby-print");
  load_string(TRUE, "GemCut.commit; p 'mruby-io' => GemCut.committed?('mruby-io')", 3, "mruby-sprintf", "mruby-math", "mruby-print");
  load_string(TRUE, "GemCut.commit; p GemCut.committed_list", 1, "mruby-print");
  load_string(TRUE, "GemCut.available_list.each { |mgem| GemCut.pickup mgem }; GemCut.commit; p GemCut.committed_list", 0);

  return 0;
}
