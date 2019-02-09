#ifndef MRUBY_GEMCUT_H
#define MRUBY_GEMCUT_H 1

#include <mruby.h>
#include <mruby/error.h>

/* gem 加工 API */

/**
 * `src` が選択した gems を `dest` に転写します。
 */
MRB_API int mruby_gemcut_imitate_to(mrb_state *dest, mrb_state *src);

/**
 * 現在選択している gems をすべて破棄し、何も選択していない状態にします。
 */
MRB_API void mruby_gemcut_clear(mrb_state *mrb);

/**
 * `name` 引数に一致する gem を選択状態にします。
 *
 * `name` は大文字小文字を区別します。
 */
MRB_API int mruby_gemcut_pickup(mrb_state *mrb, const char name[]);

/**
 * 現在選択している gems を利用可能なように初期化します。
 *
 * この関数を呼び出した後は、再びこの関数を呼び出すことは出来ません。
 */
MRB_API struct RException *mruby_gemcut_commit(mrb_state *mrb);

/* 状態取得 API */

/**
 * 利用可能な、`build_config.rb` で含めてある gems の名前を配列で返します。
 */
MRB_API mrb_value mruby_gemcut_available_list(mrb_state *mrb);

/**
 * `mruby_gemcut_commit()` して初期化された gems の名前の配列を返します。
 */
MRB_API mrb_value mruby_gemcut_committed_list(mrb_state *mrb);

/**
 * 利用可能な、`build_config.rb` で含めてある gems の数を返します。
 */
MRB_API int mruby_gemcut_available_size(mrb_state *mrb);

/**
 * `mruby_gemcut_commit()` して初期化された gems の数を返します。
 */
MRB_API int mruby_gemcut_commit_size(mrb_state *mrb);

/**
 * gem が利用可能かどうか確認します。
 */
MRB_API mrb_bool mruby_gemcut_available_p(mrb_state *mrb, const char name[]);

/**
 * gem が初期化されているかどうか確認します。
 */
MRB_API mrb_bool mruby_gemcut_committed_p(mrb_state *mrb, const char name[]);

/* mruby モジュール API */

/**
 * mruby 空間から利用可能な `GemCut` モジュールを初期化します。
 */
MRB_API int mruby_gemcut_define_module(mrb_state *mrb);

#endif /* MRUBY_GEMCUT_H */
