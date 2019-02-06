# mruby-gemcut

`build_config.rb` であらかじめ組み込んだ mruby-gems を、`mrb_open_core()` した後に選り好みして取り込む仕組みを提供します。

一つのプロセス空間で必要に応じて機能を省略・制限した個別の mruby vm を構成可能です。


## だいじなおやくそく！

***`mrb_open()` や `mrb_open_alloc()` を行った場合、二重初期化を避ける目的のため mruby gemcut API を行うことが出来ません。***

***あくまでも `mrb_open_core()` した直後に `mruby_gemcut_pickup()` および `mruby_gemcut_commit()` を行って下さい。***


## できること

`mruby-sprintf` + `mruby-print` のみを組み込んだ `mrb1` と、`mrb1` に `mruby-math` を追加した `mrb2` を持つ場合でサンプルを示します。

```c
mrb_state *mrb1 = mrb_open_core(mrb_default_allocf, NULL);
mruby_gemcut_pickup(mrb1, "mruby-sprintf");
mruby_gemcut_pickup(mrb1, "mruby-print");
mruby_gemcut_commit(mrb1); /* これ以降は mrb1 に対して mruby_gemcut_pickup() を受け付けない */

mrb_state *mrb2 = mrb_open_core(mrb_default_allocf, NULL);
mruby_gemcut_imitate_to(mrb2, mrb1);  /* mrb1 と同じ mruby gems の構成にする */
mruby_gemcut_pickup(mrb2, "mruby-math");
mruby_gemcut_commit(mrb2); /* これ以降は mrb2 に対して mruby_gemcut_pickup() を受け付けない */
```

※mruby 空間で利用可能なクラス・モジュール・メソッド・定数などは定義されません。


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
  gemcut_incdir = ...
  conf.cc.include_paths << gemcut_incdir
end
```


## つかいかた

(そのうち書く)


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
