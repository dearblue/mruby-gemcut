# mruby-gemcut

`build_config.rb` であらかじめ組み込んだ mruby-gems を、`mrb_open()` や `mrb_open_core()` した後に選り好みして取り込む仕組みを提供します。

一つの実行ファイル、同じプロセス空間で必要に応じて機能を省略・制限した個別の mruby VM を構成可能です。


## だいじなおやくそく！

`mrb_open()` した後は、`mruby-gemcut` が初期化されているだけで他の GEM は利用できない状態です。
`Gemcut.require` メソッドや `mruby_gemcut_require()` 関数を呼び出して他の GEM を有効化出来ます。
例えば `Gemcut.require "mruby-print"` すれば `Kernel#puts` や `Kernel#p` などが利用できるようになります。


## できること

### gemcut C API

利用するためには `#include <mruby-gemcut.h>` を記述して下さい。

これらの API 関数は、例外が起きても問題ない場合であれば例外を起こします。
例外が発生すると `abort()` が発生する場合は、例外を起こさずに戻り値を返します。
※詳細を述べると `mrb->jmp` が設定されていれば例外を起こします。

  - gem 加工 API

      - `MRB_API mrb_value mruby_gemcut_require(mrb_state *mrb, const char *name)`

  - 状態取得 API

      - `MRB_API mrb_value mruby_gemcut_loaded_features(mrb_state *mrb)`
      - `MRB_API int mruby_gemcut_loaded_feature_count(mrb_state *mrb)`
      - `MRB_API mrb_bool mruby_gemcut_loaded_feature_p(mrb_state *mrb, const char *name)`
      - `MRB_API mrb_value mruby_gemcut_loadable_features(mrb_state *mrb)`
      - `MRB_API int mruby_gemcut_loadable_feature_count(mrb_state *mrb)`
      - `MRB_API mrb_bool mruby_gemcut_loadable_feature_p(mrb_state *mrb, const char *name)`

  - mruby モジュール API

      - `MRB_API void mruby_gemcut_lock(mrb_state *mrb)` - `mruby_gemcut_require()` 及び `Gemcut.require` を封印します。
      - `MRB_API void mruby_gemcut_seal(mrb_state *mrb)` - あらゆる gemcut mruby API の操作を封印します。

### gemcut mruby API

これらは `mrb_open()` や `mrb_open_alloc()` した場合に最初から利用できます。
`mrb_open_core()` で利用するためには、あらかじめ C 空間で `mruby_gemcut_require(mrb, "mruby-gemcut")` を呼び出しておく必要があります。

  - `module Gemcut`

      - `Gemcut.require(gemname)`
      - `Gemcut.loaded_features`
      - `Gemcut.loaded_feature_count`
      - `Gemcut.loaded_feature?(gemname)`
      - `Gemcut.loadable_features`
      - `Gemcut.loadable_feature_count`
      - `Gemcut.loadable_feature?(gemname)`
      - `Gemcut.lock` - `Gemcut.require` を封印します。
      - `Gemcut.seal` - `Gemcut.lock` に加えて、`Gemcut` モジュールを未定義にします。


## くみこみかた

mruby gem パッケージとして依存したい場合、`mrbgem.rake` に記述して下さい。

```ruby
# mrbgem.rake
MRuby::Gem::Specification.new("mruby-XXX") do |spec|
  ...
  spec.add_dependency "mruby-gemcut", mgem: "mruby-gemcut"
end
```

- - - -

`build_config.rb` に gem として追加する場合、mruby-gemcut のインクルードディレクトリを含める必要があります。

```ruby
MRuby::Build.new do |conf|
  conf.gem "mruby-gemcut", mgem: "mruby-gemcut"
  gemcut_incdir = "ディレクトリをどうにかして取得する"
  conf.cc.include_paths << gemcut_incdir
end
```

### ブラックリスト

`mruby_gemcut_require()` 関数や `Gemcut.require` メソッドによって有効化出来ない mruby gems を指定することが出来ます。

たとえば "mruby-io" と "mruby-socket" をブラックリストに追加したい場合は次のようにします:

```ruby
# build_config.rb

MRuby::Build.new do |conf|
  ...
  conf.gem "mruby-gemcut", mgem: "mruby-gemcut" do
    add_blacklist "mruby-io"
    add_blacklist "mruby-socket"
  end
end
```

この設定でビルドしたバイナリは、`mruby_gemcut_require(mrb, "mruby-io")` や `Gemcut.require("mruby-socket")` するとエラーが返ったり、例外が発生したりします。

ただし `mruby_open()` や `mruby_open_alloc()` を制限するものではないことに注意して下さい。


## つかいかた

`mruby-sprintf` + `mruby-print` のみを組み込んだ `mrb1` と、`mrb1` に `mruby-math` を追加した `mrb2` を持つ場合でサンプルを示します。

まずは使いたい mruby gems を `build_config.rb` に追加します。

```ruby
# build_config.rb

MRuby::Build.new do |conf|
  conf.toolchain :gcc
  conf.gem core: "mruby-math"
  conf.gem core: "mruby-sprintf"
  conf.gem core: "mruby-print"
  conf.gem "YOUR-BIN-TOOL"
end
```

```ruby
# YOUR-BIN-TOOL/mrbgem.rake

MRuby::Gem::Specification.new("YOUR-BIN-TOOL") do |spec|
  spec.author = "YOURNAME"
  spec.license = "NYSL" # Likely Public Domain; See http://www.kmonos.net/nysl/
  spec.add_dependency "mruby-gemcut", mgem: "mruby-gemcut"
  spec.bins = %w(YOUR-BIN)
end
```

それからあなたの実行コードに `mrb_open()` と mruby gemcut API を記述します。

```c
/* YOUR-BIN-TOOL/tools/YOUR-BIN/tool.c */

#include <mruby.h>
#include <mruby-gemcut.h>

int
main(int argc, char *argv[])
{
  mrb_state *mrb1 = mrb_open();
  mruby_gemcut_require(mrb1, "mruby-sprintf");
  mruby_gemcut_require(mrb1, "mruby-print");
  mruby_gemcut_lock(mrb1); /* これ以降は mrb1 に対して mruby_gemcut_require() を受け付けない */

  mrb_state *mrb2 = mrb_open();
  mruby_gemcut_imitate_to(mrb2, mrb1);  /* mrb1 と同じ mruby gems の構成にする */
  mruby_gemcut_require(mrb2, "mruby-math");
  mruby_gemcut_require(mrb2, "mruby-gemcut"); /* Ruby 空間で gemcut mruby API を使う場合に指定する */
  //mruby_gemcut_lock(mrb2); /* これ以降の mrb2 に対して mruby_gemcut_require() を封印したい場合に指定する */

  /*
   * mrb1 と mrb2 を面白可笑しく処理する
   */

  return 0;
}
```


## Specification

  - Package name: mruby-gemcut
  - Version: 0.3.2
  - Product quality: PROTOTYPE
  - Author: [dearblue](https://github.com/dearblue)
  - Project page: <https://github.com/dearblue/mruby-gemcut>
  - Licensing: [2 clause BSD License](LICENSE)
  - Object code size: + 10 KiB
  - Required runtime heap size: + 1 KiB
  - Dependency external mruby-gems: (NONE)
  - Dependency C libraries: (NONE)
