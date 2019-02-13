MRuby::Build.new do
  toolchain :clang
  enable_test
  enable_debug
  gem File.join(File.dirname(__FILE__), "..")
  gem core: "mruby-sprintf"
  gem core: "mruby-print"
  gem core: "mruby-math"
end
