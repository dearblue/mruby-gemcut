MRuby::Build.new do
  toolchain :clang
  enable_test
  enable_debug
  gem File.join(__dir__, "..")
end
