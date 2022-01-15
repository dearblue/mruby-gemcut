#!ruby

require "yaml"

config = YAML.load <<'YAML'
  common:
    gems:
    - :core: "mruby-sprintf"
    - :core: "mruby-print"
    - :core: "mruby-bin-mrbc"
  builds:
    nobox:
      defines: [MRB_INT32, MRB_NO_BOXING]
      gem: "mruby-string-ext"
    wordbox++:
      defines: [MRB_INT64, MRB_WORD_BOXING]
      c++abi: true
YAML

MRuby::Lockfile.disable rescue nil

buildbase = "build/gemcut-test"

MRuby::Build.new do
  toolchain :clang
  self.build_dir = "#{buildbase}/#{self.name}"
  enable_debug

  gembox "default"

  gem __dir__ do |g|
    if g.cc.command =~ /\b(?:g?cc|clang)\d*\b/
      g.cc.flags << "-std=c11"
      g.cc.flags << %w(-Wpedantic -Wall -Wextra)
      g.cxx.flags << "-std=c++11"
      g.cxx.flags << %w(-Wpedantic -Wall -Wextra)
    end
  end

  gem File.join(__dir__, "testgem")
end

config["builds"].each_pair do |n, c|
  MRuby::Build.new(n) do |conf|
    toolchain :clang

    conf.build_dir = File.expand_path(c["build_dir"] || "#{buildbase}/#{name}")

    gembox config.dig("common", "gembox") if config.dig("common", "gembox")
    gembox c["gembox"] if c["gembox"]

    enable_debug
    enable_test
    enable_bintest if Dir.pwd == MRUBY_ROOT
    enable_cxx_exception if c["c++exception"]
    enable_cxx_abi if c["c++abi"]

    cc.defines << [*c["defines"]]
    cc.flags << [*c["cflags"]]

    Array(config.dig("common", "gems")).each { |*g| gem *g }
    Array(c["gems"]).each { |*g| gem *g }

    gem __dir__ do |g|
      if g.cc.command =~ /\b(?:g?cc|clang)\d*\b/
        g.cc.flags << (c["c++abi"] ? "-std=c++11" : "-std=c11")
        g.cc.flags << %w(-Wpedantic -Wall -Wextra)
        g.cxx.flags << "-std=c++11"
        g.cxx.flags << %w(-Wpedantic -Wall -Wextra)
      end
    end

    gem File.join(__dir__, "testgem")
  end
end
