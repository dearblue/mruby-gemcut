#ifndef MRUBY_GEMCUT_H
#define MRUBY_GEMCUT_H 1

#include <mruby.h>
#include <mruby/error.h>

MRB_API int mruby_gemcut_pickup(mrb_state *mrb, const char name[]);
MRB_API struct RException *mruby_gemcut_commit(mrb_state *mrb);

#endif /* MRUBY_GEMCUT_H */
