#!ruby

require "yaml"

config = YAML.load <<'YAML'
  common:
    gems:
    - :core: "mruby-sprintf"
    - :core: "mruby-print"
    - :core: "mruby-bin-mrbc"
    - :core: "mruby-bin-mirb"
    - :core: "mruby-bin-mruby"
  builds:
    host:
      defines: MRB_INT64
      gembox: default
    #host-int32:
    #  defines: MRB_INT32
    #host-nan-int16:
    #  defines: [MRB_INT16, MRB_NAN_BOXING]
    #host+:
    #  defines: [MRB_INT64, MRB_WORD_BOXING]
    #  c++exception: true
    host++:
      defines: [MRB_INT64, MRB_WORD_BOXING]
      c++abi: true
YAML

MRuby::Lockfile.disable rescue nil

config["builds"].each_pair do |n, c|
  MRuby::Build.new(n) do |conf|
    toolchain :clang

    conf.build_dir = File.expand_path(c["build_dir"] || name)

    gembox config.dig("common", "gembox") if config.dig("common", "gembox")
    gembox c["gembox"] if c["gembox"]

    enable_debug
    enable_test
    enable_cxx_exception if c["c++exception"]
    enable_cxx_abi if c["c++abi"]

    cc.defines << [*c["defines"]]
    cc.flags << [*c["cflags"]]

    Array(config.dig("common", "gems")).each { |*g| gem *g }
    Array(c["gems"]).each { |*g| gem *g }

    gem File.dirname(__FILE__) do |g|
      include_testtools

      if g.cc.command =~ /\b(?:g?cc|clang)\d*\b/
        g.cc.flags << (c["c++abi"] ? "-std=c++11" : "-std=c11")
        g.cc.flags << %w(-Wpedantic -Wall -Wextra)
        g.cxx.flags << "-std=c++11"
        g.cxx.flags << %w(-Wpedantic -Wall -Wextra)
      end
    end
  end
end
