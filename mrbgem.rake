#!ruby

require "fileutils"
require_relative "buildlib/internals"

using Gemcut::Internals

MRuby::Gem::Specification.new("mruby-gemcut") do |s|
  s.summary = "runtime reconfigurer for mruby gems"
  version = File.read(File.join(File.dirname(__FILE__), "README.ja.md")).scan(/^ *[-*] version: *(\d+(?:.\w+)+)/i).flatten[-1] rescue nil
  s.version = version if version
  s.license = "BSD-2-Clause"
  s.author  = "dearblue"
  s.homepage = "https://github.com/dearblue/mruby-gemcut"

  # for `mrb_protect()`
  add_dependency "mruby-error", core: "mruby-error" if Gemcut.need_error_gem?

  class << self
    def add_blacklist(mgem)
      @blacklist << mgem
      self
    end
  end

  @blacklist = []

  if cc.command =~ /\b(?:g?cc|clang)d*\b/
    cc.flags << %w(-Wno-declaration-after-statement)
  end

  make_depsfile_task
  make_geminit_task
end
