#ifndef MRUBY_GEMCUT_H
#define MRUBY_GEMCUT_H 1

#include <mruby.h>
#include <mruby/error.h>

MRB_API void mruby_gemcut_clear(mrb_state *mrb);
MRB_API int mruby_gemcut_pickup(mrb_state *mrb, const char name[]);
MRB_API int mruby_gemcut_imitate_to(mrb_state *dest, mrb_state *src);
MRB_API struct RException *mruby_gemcut_commit(mrb_state *mrb);

MRB_API mrb_value mruby_gemcut_available_list(mrb_state *mrb);
MRB_API mrb_value mruby_gemcut_committed_list(mrb_state *mrb);
MRB_API int mruby_gemcut_available_size(mrb_state *mrb);
MRB_API int mruby_gemcut_commit_size(mrb_state *mrb);
MRB_API mrb_bool mruby_gemcut_available_p(mrb_state *mrb, const char name[]);
MRB_API mrb_bool mruby_gemcut_committed_p(mrb_state *mrb, const char name[]);

MRB_API int mruby_gemcut_define_module(mrb_state *mrb);

#endif /* MRUBY_GEMCUT_H */
