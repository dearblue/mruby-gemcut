#ifndef MRUBY_GEMCUT_H
#define MRUBY_GEMCUT_H 1

#include <mruby.h>
#include <mruby/error.h>

MRB_BEGIN_DECL

/* oneshot gem cut API */

/**
 * この補助関数は新しい mruby vm を新たに作成し、複数の gem をまとめてコミットして、作成した mruby vm を返します。
 *
 * gemcut のための模倣したい mruby vm を与えた場合、`mruby_gemcut_imitate_to()` が呼ばれます。
 *
 * 処理の流れとしては次のようになります:
 *
 * * `mrb_open_core()`
 * * `mruby_gemcut_imitate_to()`
 * * `mruby_gemcut_pickup()`
 * * `mruby_gemcut_commit()`
 * * return with `mrb_state *`
 */
MRB_API mrb_state *mruby_gemcut_open(mrb_state *imitate_src, int num_names, const char *name_table[], mrb_allocf allocator, void *alloc_udata);

/**
 * この補助関数は新しい mruby vm を新たに作成し、gem cut を行う前に mruby バイナリを実行して、作成した mruby vm を返します。
 *
 * gemcut のための模倣したい mruby vm を与えた場合、事前に `mruby_gemcut_imitate_to()` が呼ばれます。
 *
 * 処理の流れとしては次のようになります:
 *
 * * `mrb_open_core()`
 * * `mruby_gemcut_imitate_to()`
 * * `mruby_gemcut_define_module()`
 * * `mrb_load_irep()`
 * * `mruby_gemcut_commit()`
 * * return with `mrb_state *`
 */
MRB_API mrb_state *mruby_gemcut_open_mrb(mrb_state *imitate_src, const void *bin, size_t binsize, mrb_allocf allocator, void *alloc_udata);

/* Gemcut Model API */

/**
 * `model_name` を適用します。
 *
 * `mruby_gemcut_pickup()` を先に呼び出してある場合、状態が取り消されることに注意して下さい。
 *
 * Gemcut モデルを初期状態 (選択なし) にしたい場合は `mruby_gemcut_clear()` 関数を呼んで下さい。
 */
MRB_API int mruby_gemcut_model_select(mrb_state *mrb, const char model_name[]);

/**
 * 現在適用中の Gemcut モデル名を取得します。
 */
MRB_API const char *mruby_gemcut_model_name(mrb_state *mrb);

/**
 * ビルド時に組み込まれた Gemcut モデル名の一覧を配列にして返します。
 */
MRB_API mrb_value mruby_gemcut_model_list(mrb_state *mrb);

/**
 * ビルド時に組み込まれた Gemcut モデルの要素数を返します。
 */
MRB_API size_t mruby_gemcut_model_size(mrb_state *mrb);

/**
 * 引数で指定した gemcut model が利用可能かどうかを返します。
 */
MRB_API mrb_bool mruby_gemcut_model_p(mrb_state *mrb, const char model_name[]);

/* gem 加工 API */

/**
 * `src` が選択した gems を `dest` に転写します。
 */
MRB_API int mruby_gemcut_imitate_to(mrb_state *dest, mrb_state *src);

/**
 * 現在選択している gems をすべて破棄し、何も選択していない状態にします。
 *
 * また、gemcut model を初期状態にします。
 */
MRB_API void mruby_gemcut_clear(mrb_state *mrb);

/**
 * `name` 引数に一致する gem を選択状態にします。
 *
 * `name` は大文字小文字を区別します。
 */
MRB_API int mruby_gemcut_pickup(mrb_state *mrb, const char name[]);

/**
 * 複数の gem 名をまとめて与えることの出来る `mruby_gemcut_pickup()` 関数です。
 *
 * 途中でエラーが起きても続行しますが、その場合は `1` を返します。
 *
 * 正常であれば `0` を返します。
 */
MRB_API int mruby_gemcut_pickup_multi(mrb_state *mrb, int num_names, const char *name_table[]);

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
 *
 * `name` に対して `NULL` を与えた場合、コミットされていれば真を、コミット前であれば偽を返します。
 */
MRB_API mrb_bool mruby_gemcut_committed_p(mrb_state *mrb, const char name[]);

/* mruby モジュール API */

/**
 * mruby 空間から利用可能な `GemCut` モジュールを初期化します。
 */
MRB_API int mruby_gemcut_define_module(mrb_state *mrb);

MRB_END_DECL

#endif /* MRUBY_GEMCUT_H */
