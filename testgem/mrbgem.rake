MRuby::Gem::Specification.new("test-mruby-gemcut") do |s|
  s.summary = "bintest for mruby-gemcut"
  s.licenses = [
    "NASL",  # likely Public Domain; See http://www.kmonos.net/nysl/
    "WTFPL", # likely Public Domain; See http://www.wtfpl.net/
  ]
  s.author  = "dearblue"
  s.homepage = "https://github.com/dearblue/mruby-gemcut"

  add_dependency "mruby-gemcut"
  add_dependency "mruby-compiler", core: "mruby-compiler"
  add_dependency "mruby-print", core: "mruby-print"
  add_dependency "mruby-sprintf", core: "mruby-sprintf"
  add_dependency "mruby-math", core: "mruby-math"

  build.cc.include_paths << (File.join(__dir__, "../include")) if MRuby::Source::MRUBY_RELEASE_NO < 30000

  s.bins = %w(mruby-gemcut-test)
end
