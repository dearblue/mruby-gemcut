# mruby-gemcut

`build_config.rb` であらかじめ組み込んだ mruby-gems を、`mrb_open_core()` した後に選り好みして取り込む仕組みを提供します。

一つの実行ファイル、同じプロセス空間で必要に応じて機能を省略・制限した個別の mruby vm を構成可能です。


## だいじなおやくそく！

***`mrb_open()` や `mrb_open_alloc()` を行った場合、二重初期化を避ける目的のため mruby gemcut API を行うことが出来ません。***

***あくまでも `mrb_open_core()` した直後に `mruby_gemcut_pickup()` および `mruby_gemcut_commit()` を行って下さい。***


## できること

### gemcut C API

  - gem 加工 API
      - `MRB_API int mruby_gemcut_imitate_to(mrb_state *dest, mrb_state *src);`  
        `src` が選択した gems を `dest` に転写します。
      - `MRB_API void mruby_gemcut_clear(mrb_state *mrb);`  
        現在選択している gems をすべて破棄し、何も選択していない状態にします。
      - `MRB_API int mruby_gemcut_pickup(mrb_state *mrb, const char name[]);`  
        `name` 引数に一致する gem を選択状態にします。  
        `name` は大文字小文字を区別します。
      - `MRB_API struct RException *mruby_gemcut_commit(mrb_state *mrb);`  
        現在選択している gems を利用可能なように初期化します。  
        この関数を呼び出した後は、再びこの関数を呼び出すことは出来ません。  
        初期化中に発生した例外は内部で捕捉し戻り値として返します。
  - 状態取得 API
      - `MRB_API mrb_value mruby_gemcut_available_list(mrb_state *mrb);`  
        利用可能な、`build_config.rb` で含めてある gems の名前を配列で返します。
      - `MRB_API mrb_value mruby_gemcut_committed_list(mrb_state *mrb);`  
        `mruby_gemcut_commit()` して初期化された gems の名前の配列を返します。
      - `MRB_API int mruby_gemcut_available_size(mrb_state *mrb);`  
        利用可能な、`build_config.rb` で含めてある gems の数を返します。
      - `MRB_API int mruby_gemcut_commit_size(mrb_state *mrb);`  
        `mruby_gemcut_commit()` して初期化された gems の数を返します。
      - `MRB_API mrb_bool mruby_gemcut_available_p(mrb_state *mrb, const char name[]);`  
        gem が利用可能かどうか確認します。
      - `MRB_API mrb_bool mruby_gemcut_committed_p(mrb_state *mrb, const char name[]);`  
        gem が初期化されているかどうか確認します。
  - mruby モジュール API
      - `MRB_API int mruby_gemcut_define_module(mrb_state *mrb);`  
        mruby 空間から利用可能な `GemCut` モジュールを初期化します。  
        gemcut mruby API を用いたい場合はこの関数の呼び出しが必要です。

### gemcut mruby API

  - `module GemCut`
      - `GemCut.pickup(gemname)`
      - `GemCut.commit`
      - `GemCut.available_list`
      - `GemCut.committed_list`
      - `GemCut.available_sise`
      - `GemCut.commit_size`
      - `GemCut.available?(gemname)`
      - `GemCut.committed?(gemname)`


## くみこみかた

mruby gem パッケージとして依存したい場合、`mrbgem.rake` に記述して下さい。

```ruby
# mrbgem.rake
MRuby::Gem::Specification.new("mruby-XXX") do |spec|
  ...
  spec.add_dependency "mruby-gemcut", github: "dearblue/mruby-gemcut"
end
```

- - - -

`build_config.rb` に gem として追加する場合、mruby-gemcut のインクルードディレクトリを含める必要があります。

```ruby
MRuby::Build.new do |conf|
  conf.gem "mruby-gemcut", github: "dearblue/mruby-gemcut"
  gemcut_incdir = "ディレクトリをどうにかして取得する"
  conf.cc.include_paths << gemcut_incdir
end
```


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
  spec.add_dependency "mruby-gemcut", github: "dearblue/mruby-gemcut"
  spec.bins = %w(YOUR-BIN)
end
```

それからあなたの実行コードに `mrb_open_core()` と mruby gemcut API を記述します。

```c
/* YOUR-BIN-TOOL/tools/YOUR-BIN/tool.c */

#include <mruby.h>
#include <mruby-gemcut.h>

int
main(int argc, char *argv[])
{
  mrb_state *mrb1 = mrb_open_core(mrb_default_allocf, NULL);
  mruby_gemcut_pickup(mrb1, "mruby-sprintf");
  mruby_gemcut_pickup(mrb1, "mruby-print");
  mruby_gemcut_commit(mrb1); /* これ以降は mrb1 に対して mruby_gemcut_pickup() を受け付けない */

  mrb_state *mrb2 = mrb_open_core(mrb_default_allocf, NULL);
  mruby_gemcut_imitate_to(mrb2, mrb1);  /* mrb1 と同じ mruby gems の構成にする */
  mruby_gemcut_pickup(mrb2, "mruby-math");
  mruby_gemcut_commit(mrb2); /* これ以降は mrb2 に対して mruby_gemcut_pickup() を受け付けない */

  /*
   * mrb1 と mrb2 を面白可笑しく処理する
   */

  return 0;
}
```


## Specification

  - Package name: mruby-gemcut
  - Product quality: PROTOTYPE
  - Author: [dearblue](https://github.com/dearblue)
  - Project page: <https://github.com/dearblue/mruby-gemcut>
  - Licensing: [2 clause BSD License](LICENSE)
  - Object code size: + 10 KiB
  - Required runtime heap size: + 1 KiB
  - Dependency external mruby-gems: (NONE)
  - Dependency C libraries: (NONE)
