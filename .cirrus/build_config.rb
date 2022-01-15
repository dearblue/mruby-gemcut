MRuby::Build.new do
  toolchain :clang
  enable_bintest
  enable_test
  enable_debug
  gem File.join(__dir__, "..")
  gem File.join(__dir__, "../testgem")
  gem core: "mruby-compiler"
  gem core: "mruby-sprintf"
  gem core: "mruby-print"
  gem core: "mruby-math"
end
