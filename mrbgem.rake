#!ruby

require "fileutils"
require_relative "helper/internals"

using Gemcut::Internals

MRuby::Build.current.instance_eval do
  if self.test_enabled?
    # もしかしたら問題があるかもしれない
    self.gem core: "mruby-compiler"
    self.gem core: "mruby-enumerator"
    self.gem core: "mruby-fiber"
    self.gem core: "mruby-hash-ext"
    self.gem core: "mruby-sprintf"
  end
end

MRuby::Gem::Specification.new("mruby-gemcut") do |s|
  s.summary = "runtime reconfigurer for mruby gems"
  version = File.read(File.join(File.dirname(__FILE__), "README.ja.md")).scan(/^ *[-*] version: *(\d+(?:.\w+)+)/i).flatten[-1] rescue nil
  s.version = version if version
  s.license = "BSD-2-Clause"
  s.author  = "dearblue"
  s.homepage = "https://github.com/dearblue/mruby-gemcut"

  build.cc.include_paths << File.join(__dir__, "include") if MRuby::Source::MRUBY_RELEASE_NO < 30000

  # for `mrb_protect()`
  add_dependency "mruby-error", core: "mruby-error" if Gemcut.need_error_gem?

  class << self
    def add_denylist(*gems)
      gems.flatten!
      gems.each { |e| e.ensure_string }
      @models[0].deny.concat gems
      self
    end
    alias add_blacklist add_denylist

    def add_model(name, bundle: nil, allow: nil, deny: nil)
      name = name.ensure_string
      raise NameError, "bad empty `name` for model" if name.empty?
      raise NameError, "already exist model `name` - #{name}" if @models.find { |m| m.name == name }

      bundle = bundle.ensure_array_or_nil
      allow = allow.ensure_array_or_state
      deny = deny.ensure_array_or_state
      raise ArgumentError, "need the `bundle`, `allow` or `deny` arguments" if bundle.empty? && allow.empty? && deny.empty?
      raise ArgumentError, "the `allow` and `deny` arguments are exclusive" unless allow.empty? || deny.empty?

      case
      when allow == true
        deny = false
      when allow == false
        deny = true
      when deny == true
        allow = false
      when deny == false
        allow = true
      end

      @models << Gemcut::Model.new(name, bundle, allow, deny, caller)

      self
    end
  end

  @models = [Gemcut::Model.new(nil, ["mruby-gemcut"], [], [], caller)]

  if cc.command =~ /\b(?:g?cc|clang)d*\b/
    cc.flags << %w(-Wno-declaration-after-statement)
  end

  make_depsfile_task
  make_geminit_task
end

gems = MRuby::Build.current.gems.instance_eval { @ary }
if irequire = gems.find_index { |e| e.name == "mruby-require" }
  unless igemcut = gems.find_index { |e| e.name == "mruby-gemcut" }
    gems.insert(irequire, MRuby::Gem.current)
    $stderr.puts %(\e[7mwarning\e[m: mruby-gemcut has been replaced so that it precedes mruby-require in order.)
  end
end
