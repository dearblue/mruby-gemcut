MRuby::Build.new do
  toolchain :clang
  #enable_bintest
  enable_test
  enable_debug
  gem File.join(__dir__, "..") do include_testtools end
  gem File.join(__dir__, "../.testset")
  gem core: "mruby-compiler"
  gem core: "mruby-sprintf"
  gem core: "mruby-print"
  gem core: "mruby-math"
end
