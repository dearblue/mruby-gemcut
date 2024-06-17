#ifndef MRUBY_GEMCUT_H
#define MRUBY_GEMCUT_H 1

#include <mruby.h>
#include <mruby/error.h>

MRB_BEGIN_DECL

/* Gemcut Model API */

/**
 * `model_name` を適用します。
 *
 * `mrb_open_core()` した後に呼び出すことが出来ます。
 *
 * すでに何らかの GEM を初期化している場合、この関数は失敗します。
 * これは `mrb_open()` したあとに使うことは出来ないことを意味します。
 *
 * 成功すれば 0 を、失敗すれば 0 ではない値を返します。
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
 * 引数 +name+ に対する gem を初期化し、利用可能な状態にします。
 * 依存関係にある gem も同時に初期化されます。
 *
 * 初期化に成功すれば +true+ を返します。すでに初期化されている場合は +false+ を返します。
 *
 * この関数は例外を発生させる場合がありますが、<tt>mrb->jmp == NULL</tt> の場合は発生した例外オブジェクトを返します。
 */
MRB_API mrb_value mruby_gemcut_require(mrb_state *mrb, const char *name);

/**
 * +src+ で有効化されている gems を +dest+ でも利用可能なように写します。
 * すでに初期化されている gems はそのまま利用可能です。
 *
 * 成功すれば +nil+ を返します。
 *
 * この関数は例外を発生させる場合がありますが、<tt>mrb->jmp == NULL</tt> の場合は発生した例外オブジェクトを返します。
 */
MRB_API mrb_value mruby_gemcut_imitate_to(mrb_state *dest, mrb_state *src);

/* 状態取得 API */

/**
 * +Gemcut.require+ して有効となった gem 名の配列を返します。
 *
 * この関数は例外を発生させる場合がありますが、<tt>mrb->jmp == NULL</tt> の場合は +nil+ を返します。
 */
MRB_API mrb_value mruby_gemcut_loaded_features(mrb_state *mrb);

/**
 * +Gemcut.require+ して有効となった gem の数を返します。
 *
 * この関数は例外を発生させる場合がありますが、<tt>mrb->jmp == NULL</tt> の場合は +-1+ を返します。
 */
MRB_API int mruby_gemcut_loaded_count(mrb_state *mrb);

/**
 * 引数 +name+ に一致する gem が +Gemcut.require+ によって有効化しているかどうかを真偽値で返します。
 *
 * この関数は例外を発生させる場合がありますが、<tt>mrb->jmp == NULL</tt> の場合は +false+ を返します。
 */
MRB_API mrb_bool mruby_gemcut_loaded_p(mrb_state *mrb, const char *name);

/**
 * +Gemcut.require+ して利用可能に出来る gem 名の配列を返します。
 *
 * この関数は例外を発生させる場合がありますが、<tt>mrb->jmp == NULL</tt> の場合は +nil+ を返します。
 */
MRB_API mrb_value mruby_gemcut_loadable_features(mrb_state *mrb);

/**
 * +Gemcut.require+ して利用可能に出来る gem の数を返します。
 *
 * この関数は例外を発生させる場合がありますが、<tt>mrb->jmp == NULL</tt> の場合は +-1+ を返します。
 */
MRB_API int mruby_gemcut_loadable_count(mrb_state *mrb);

/**
 * 引数 +name+ に一致する gem が +Gemcut.require+ によって有効化可能かどうかを真偽値で返します。
 *
 * この関数は例外を発生させる場合がありますが、<tt>mrb->jmp == NULL</tt> の場合は +false+ を返します。
 */
MRB_API mrb_bool mruby_gemcut_loadable_p(mrb_state *mrb, const char *name);

/* mruby モジュール API */

/**
 * 以後の +mruby_gemcut_require()+ や +Gemcut.require+ を封印します。
 * 呼び出した場合は +Exception+ 例外が発生するようになります。
 * ※CRuby において +Exception+ は +SecurityError+ の親クラスです。
 *
 * この関数は例外を発生させる場合がありますが、<tt>mrb->jmp == NULL</tt> の場合は制御を関数の呼び出し元に戻します。
 */
MRB_API void mruby_gemcut_lock(mrb_state *mrb);

/**
 * 以後の +mruby_gemcut_require()+ や +Gemcut.require+ を封印します。
 * また、+Gemcut+ モジュールの状態取得 API メソッドも封印されます。
 * 呼び出した場合は +Exception+ 例外が発生するようになります。
 * ※CRuby において +Exception+ は +SecurityError+ の親クラスです。
 *
 * この関数は例外を発生させる場合がありますが、<tt>mrb->jmp == NULL</tt> の場合は制御を関数の呼び出し元に戻します。
 */
MRB_API void mruby_gemcut_seal(mrb_state *mrb);

MRB_END_DECL

#endif /* MRUBY_GEMCUT_H */
