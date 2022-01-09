# Gemcut Model API

この機能は以前追加したブラックリスト (禁止リスト) の機能を拡大したものです。
利用可・禁止する mruby gems をまとめて管理し、複数の管理単位を実行時に切り替える仕組みを追加しました。
この仕組みのことを Gemcut Model API としています。

ここで出てきた管理単位は gemcut model としています。

gemcut model には名前が必要です。名前は利用者の自由ですが、無名を付けることは出来ません。


## つかいかた

利用手続きの大まかな流れは次のようになります:

 1. ビルド設定ファイル (例えば `build_config.rb`) で `conf.gem mgem: "mruby-gemcut"` を記述し、ブロック引数を与えて詳細を与えます。
 2. C 関数で `mrb_open_core()` 関数の後、`mruby_gemcut_pickup()` 関数の前に `mruby_gemcut_model_select()` 関数を呼びます。
 3. `mruby_gemcut_pickup()` 関数で追加の利用したい gems を指定して、`mruby_gemcut_commit()` 関数を呼んで下さい。

### (1) ビルド設定ファイル

ブロックの中で、`add_model` メソッドを使います。

```ruby
MRuby::Build.new do |conf|
  ...SNIP...
  conf.gem mgem: "mruby-gemcut" do |gemconf|
    gemconf.add_model "name1", bundle: bundle_list
    gemconf.add_model "name2", bundle: bundle_list, pass: pass_list
    gemconf.add_model "name3", bundle: bundle_list, drop: drop_list
    gemconf.add_model "name4", pass: pass_list
    gemconf.add_model "name5", drop: drop_list
  end
  ...SNIP...
end
```

`add_model` メソッドのキーワード引数の組み合わせは5つになります。
3つのうちどれかを指定する必要があり、かつ `pass` と `drop` キーワードを同時に指定することは出来ません。

`name` 引数は文字列に限定されます。空の文字列 (`""`) を与えることは出来ません。

`bundle` キーワード引数に文字列の配列を与えると、モデルを適用したと同時に選択状態としたい mruby gems を指定できます。

`pass` キーワード引数に文字列の配列を与えると、任意に有効化したい mruby gems を指定できます。
`true` を与えると組み込まれた全ての mruby gems を任意に有効化出来ます。
`false` を与えると `bundle` に指定した mruby gems 以外の利用を禁止にします。

`drop` キーワード引数に文字列の配列を与えると、利用を禁止したい mruby gems を指定できます。
`true` を与えると `bundle` に指定した mruby gems 以外の利用を禁止にします (`pass: false` と等価)。
`false` を与えると組み込まれた全ての mruby gems を任意に有効化出来ます (`pass: true` と等価)。

`pass` と `drop` の両方が指定されていないならば、`pass: true` として扱われます。

`bundle_list | pass_list` の集合が `gemconf.add_drop | drop_list` の集合と部分的に重なる場合、例外が発生します。

`rake check-mruby-gemcut` タスクによりコンパイルを伴わない確認が行なえるのでご活用下さい。

### (2) C 関数で gemcut model を指定する

`mrb_open_core()` 関数を呼び出したあと、gemcut model を指定することになります。
指定するには、`mruby_gemcut_model_select()` 関数を呼びます。

```c
mrb_state *mrb = mrb_open_core(NULL, NULL);
mruby_gemcut_model_select(mrb, "name1");
```

### (3) ピックアップ & コミット

あとは gemcut model を使わない場合と同じ処理を行います。
すなわち、`mruby_gemcut_pickup()` 関数によって追加で有効化したい mruby gems を指定して、`mruby_gemcut_commit()` 関数を呼ぶことです。

```c
mruby_gemcut_pickup(mrb, "mruby-array-ext");
mruby_gemcut_pickup(mrb, "mruby-hash-ext");
mruby_gemcut_pickup(mrb, "mruby-string-ext");
mruby_gemcut_commit(mrb);
```
