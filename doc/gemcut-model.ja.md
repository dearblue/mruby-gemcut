Gemcut Model API
========================================================================

この機能は以前追加したブラックリスト (禁止リスト) の機能を拡大したものです。
利用可・禁止する mruby gems をまとめて管理します。
この管理単位のことを Gemcut Model としています。

複数の管理単位を mruby 初期化 (`mrb_open_core()`) の直後に切り替える仕組みを追加しました。
この仕組みのことを Gemcut Model API としています。

Gemcut Model には名前が必要です。名前は利用者の自由ですが、無名を付けることは出来ません。
名前に利用できる文字コードは `NUL` 以外の全てです。文字エンコーディングの指定はなく、常に `ASCII-8BIT` 相当として扱われます。


つかいかた
------------------------------------------------------------------------

利用手続きの大まかな流れは次のようになります:

 1. ビルド設定ファイル (例えば `build_config.rb`) で `conf.gem mgem: "mruby-gemcut"` を記述し、ブロック引数を与えて詳細を与えます。
 2. C 関数で `mrb_open_core()` 関数の後、`mruby_gemcut_require()` 関数の前に `mruby_gemcut_model_select()` 関数を呼びます。
 3. `mruby_gemcut_require()` 関数で追加の利用したい gems を指定して下さい。

### (1) ビルド設定ファイル

ブロックの中で、`add_model` メソッドを使います。
`add_model` メソッドのキーワード引数の組み合わせは5つになります。

```ruby
MRuby::Build.new do |conf|
  ...SNIP...
  conf.gem mgem: "mruby-gemcut" do |gem|
    gem.add_model "name1", allow: allow_list
    gem.add_model "name2", deny: deny_list
    gem.add_model "name3", bundle: bundle_list
    gem.add_model "name4", bundle: bundle_list, allow: allow_list
    gem.add_model "name5", bundle: bundle_list, deny: deny_list
  end
  ...SNIP...
end
```

キーワード引数は3つのうちどれかを指定する必要があり、かつ `allow` と `deny` キーワードを同時に指定することは出来ません。

`name` 引数は文字列に限定されます。空の文字列 (`""`) を与えることは出来ません。

`bundle` キーワード引数に文字列の配列を与えると、モデルを適用したと同時に使用可能な状態としたい mruby gems を指定できます。

`allow` キーワード引数に文字列の配列を与えると、任意に有効化したい mruby gems を指定できます。
`true` を与えると組み込まれた全ての mruby gems を任意に有効化出来ます。
`false` を与えると `bundle` に指定した mruby gems 以外の利用を禁止にします。

`deny` キーワード引数に文字列の配列を与えると、利用を禁止したい mruby gems を指定できます。
`true` を与えると `bundle` に指定した mruby gems 以外の利用を禁止にします (`allow: false` と等価)。
`false` を与えると組み込まれた全ての mruby gems を任意に有効化出来ます (`allow: true` と等価)。

`allow` と `deny` の両方が指定されていないならば、`allow: true` として扱われます。

`bundle_list | allow_list` の集合が `gem.add_deny | deny_list` の集合と部分的に重なる場合、例外が発生します。

`rake check-mruby-gemcut` タスクによりコンパイルを伴わない確認が行なえるのでご活用下さい。

### (2) C 関数で Gemcut Model を指定する

`mrb_open_core()` 関数を呼び出したあと、Gemcut Model を指定することになります。
指定するには、`mruby_gemcut_model_select()` 関数を呼びます。

```c
mrb_state *mrb = mrb_open_core(NULL, NULL);
mruby_gemcut_model_select(mrb, "name1");
```

`mrb_open()` あるいは `mrb_open_alloc()` 関数によって初期化した場合、`mruby_gemcut_model_select(mrb, NULL)` によって初期化された状態になります。
`mruby` コマンドや `mirb` コマンドなどもこれに当てはまります。

どちらにしても `mruby_gemcut_model_name()` で取得できる Gemcut Model 名 は `NULL` です。

### (3) 利用したい GEM の初期化

あとは Gemcut Model を使わない場合と同じ処理を行います。
すなわち、`mruby_gemcut_require()` 関数によって追加で有効化したい mruby gems を指定することです。

```c
mruby_gemcut_require(mrb, "mruby-array-ext");
mruby_gemcut_require(mrb, "mruby-hash-ext");
mruby_gemcut_require(mrb, "mruby-string-ext");
```
