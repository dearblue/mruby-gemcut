# mruby-gemcut

`mruby-gemcut` provide a mechanism to selectively incorporate mruby-gems pre-installed in `build_config.rb` after `mrb_open_core()`.

It is possible to configure individual mruby vm which omits / restricts functions in one executable file and the same process space as necessary.

([原文である日本語 README / Original Japanese version README](README.ja.md))


## THE GREATEST PLEASURES!

***When doing `mrb_open()` or `mrb_open_alloc()` you can not do mruby gemcut API for the purpose of avoiding double initialization.***

***Please do `mruby_gemcut_pickup()` and `mruby_gemcut_commit()` just after doing `mrb_open_core ()`.***


## What you can do

### gemcut C API

  - gem processing API
      - `MRB_API int mruby_gemcut_imitate_to (mrb_state * dest, mrb_state * src);`  
        `src` transfers the selected gems to` dest`.
      - `MRB_API void mruby_gemcut_clear (mrb_state * mrb);`  
        Discards all currently selected gems and makes nothing selected.
      - `MRB_API int mruby_gemcut_pickup (mrb_state * mrb, const char name []);`  
        Select a gem that matches the `name` argument.  
        `name` is case sensitive.
      - `MRB_API struct RException * mruby_gemcut_commit (mrb_state * mrb);`  
        Initialize the currently selected gems so that it can be used.  
        After calling this function, you can not call this function again.  
        Exceptions that occurred during initialization are internally captured and returned as return values.
  - State acquisition API
      - `MRB_API mrb_value mruby_gemcut_available_list (mrb_state * mrb);`  
        Returns an array of the names of available gems included in `build_config.rb`.
      - `MRB_API mrb_value mruby_gemcut_committed_list (mrb_state * mrb);`  
        `mruby_gemcut_commit ()` and returns an array of names of gems initialized.
      - `MRB_API int mruby_gemcut_available_size (mrb_state * mrb);`  
        Returns the number of available gems included in `build_config.rb`.
      - `MRB_API int mruby_gemcut_commit_size (mrb_state * mrb);`  
        `mruby_gemcut_commit ()` and returns the number of initialized gems.
      - `MRB_API mrb_bool mruby_gemcut_available_p (mrb_state * mrb, const char name []);`  
        Make sure that gem is available.
      - `MRB_API mrb_bool mruby_gemcut_committed_p (mrb_state * mrb, const char name []);`  
        Make sure that gem is initialized.
  - mruby module API
      - `MRB_API int mruby_gemcut_define_module (mrb_state * mrb);`  
        Initialize the available `GemCut` module from mruby space.  
        If you want to use the gemcut mruby API, you need to call this function.

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


## How to install

If you want to depend on mruby gem package, write it in `mrbgem.rake`.

```ruby
# mrbgem.rake
MRuby::Gem::Specification.new("mruby-XXX") do |spec|
  ...
  spec.add_dependency "mruby-gemcut", github: "dearblue/mruby-gemcut"
end
```

- - - -

When adding as gem to `build_config.rb`, you need to include the include directory of mruby-gemcut.

```ruby
MRuby::Build.new do |conf|
  conf.gem "mruby-gemcut", github: "dearblue/mruby-gemcut"
  gemcut_incdir = "<Fetch the directory somehow>"
  conf.cc.include_paths << gemcut_incdir
end
```


## How to use

A sample is shown with `mrb1` including only` mruby-sprintf` + `mruby-print` and` mrb2` with `mruby-math` added to` mrb1`.

First, add mruby gems you want to use to `build_config.rb`.

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

Then write `mrb_open_core ()` and mruby gemcut API in your execution code.

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
  mruby_gemcut_commit(mrb1); /* From this point on, mruby_gemcut_pickup () will not be accepted for mrb1 */

  mrb_state *mrb2 = mrb_open_core(mrb_default_allocf, NULL);
  mruby_gemcut_imitate_to(mrb2, mrb1);  /* Make the same mruby gems configuration as mrb1 */
  mruby_gemcut_pickup(mrb2, "mruby-math");
  mruby_gemcut_commit(mrb2); /* From this point on, mruby_gemcut_pickup () will not be accepted for mrb2 */

  /*
   * Process mrb1 and mrb2 amusingly funny
   */

  return 0;
}
```


## Specification

  - Package name: mruby-gemcut
  - Version: 0.2
  - Product quality: PROTOTYPE
  - Author: [dearblue](https://github.com/dearblue)
  - Project page: <https://github.com/dearblue/mruby-gemcut>
  - Licensing: [2 clause BSD License](LICENSE)
  - Object code size: + 10 KiB
  - Required runtime heap size: + 1 KiB
  - Dependency external mruby-gems: (NONE)
  - Dependency C libraries: (NONE)
