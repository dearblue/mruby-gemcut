MRuby::Gem::Specification.new("test-mruby-gemcut") do |s|
  s.summary = "bintest for mruby-gemcut"
  s.licenses = [
    "NASL",  # likely Public Domain; See http://www.kmonos.net/nysl/
    "WTFPL", # likely Public Domain; See http://www.wtfpl.net/
  ]
  s.author  = "dearblue"
  s.homepage = "https://github.com/dearblue/mruby-gemcut"

  add_dependency "mruby-gemcut"
end
