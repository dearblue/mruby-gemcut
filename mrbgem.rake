#!ruby

require "fileutils"

MRuby::Gem::Specification.new("mruby-gemcut") do |s|
  s.summary = "startup installer for mruby gems"
  version = File.read(File.join(File.dirname(__FILE__), "README.md")).scan(/^ *[-*] version: *(\d+(?:.\w+)+)/i).flatten[-1] rescue nil
  s.version = version if version
  s.license = "BSD-2-Clause"
  s.author  = "dearblue"
  s.homepage = "https://github.com/dearblue/mruby-gemcut"

  # for `mrb_protect()`
  add_dependency "mruby-error", core: "mruby-error"

  #add_test_dependency "mruby-research", mgem: "mruby-research"

  if build.test_enabled? || build.bintest_enabled?
    # TODO: そのうち bintest も書く

    s.bins = %w(
      mruby-gemcut-test
    )
  end

  if cc.command =~ /\b(?:g?cc|clang)d*\b/
    cc.flags << %w(-Wno-declaration-after-statement)
  end


  # ここから下は全部 mruby-gemcut/deps.h のためのコード

  hdrgendir = File.join(build_dir, "include")
  deps_h = File.join(hdrgendir, "mruby-gemcut/deps.h")
  gemcut_o = File.join(build_dir, "src/mruby-gemcut.c").ext(exts.object)
  cc.include_paths << hdrgendir
  file gemcut_o => [File.join(dir, "src/mruby-gemcut.c"), deps_h]
  file deps_h => [__FILE__, File.join(build.build_dir, "mrbgems/gem_init.c")] do |t|
    # NOTE: file タスク中であれば build.gems はすでに依存関係が解決されている状態。

    verbose = Rake.respond_to?(:verbose) ? Rake.verbose : $-v
    puts %(GEN   #{t.name}#{verbose ? " (by #{__FILE__})" : nil}\n)

    gindex = {}
    gems = build.gems.map.with_index do |g, i|
      name = g.name.to_s
      cname = name.gsub(/[^0-9A-Za-z_]+/, "_")
      gindex[name] = i
      [name, cname, g, g.dependencies.map { |e| e[:gem].to_s }]
    end

    FileUtils.mkpath File.dirname t.name
    File.write t.name, <<-"DEPS_H", mode: "wb"
/*
 * This file is auto generated by mruby-gemcut.
 * THE DATA YOU CHANGED WILL BE LOST.
 */

/* NOTE: `struct mgem_spec` is defined in `mruby-gemcut/src/mruby-gemcut.c` */

#define MGEMS_POPULATION #{gems.size}
#define MGEMS_BITMAP_UNITS #{gems.empty? ? 1 : (gems.size + 31) / 32}

#{
  gems.map { |name, cname, gem, deps|
    if gem.generate_functions
      %(void GENERATED_TMP_mrb_#{cname}_gem_init(mrb_state *);\nvoid GENERATED_TMP_mrb_#{cname}_gem_final(mrb_state *);)
    else
      nil
    end
  }.compact.join("\n")
}

#ifdef __cplusplus
# define DEPSREF(N)         N()
# define DEPSDECL(N, E)     static const struct mgem_spec *const *N()
# define DEPSDEF(N, E, ...) static const struct mgem_spec *const *N() { static const struct mgem_spec *const deps[E] = { __VA_ARGS__ }; return deps; }
#else
# define DEPSREF(N)         N
# define DEPSDECL(N, E)     static const struct mgem_spec *const N[E]
# define DEPSDEF(N, E, ...) static const struct mgem_spec *const N[E] = { __VA_ARGS__ };
#endif

#{
  gems.map { |name, cname, gem, deps|
    if deps.empty?
      nil
    else
      %(DEPSDECL(deps_#{cname}, #{deps.size});)
    end
  }.compact.join("\n")
}

static const struct mgem_spec mgems_list[] = {
  #{
    gems.map.with_index { |(name, cname, gem, deps), i|
      if gem.generate_functions
        init = "GENERATED_TMP_mrb_#{cname}_gem_init"
        final = "GENERATED_TMP_mrb_#{cname}_gem_final"
      else
        init = final = "NULL"
      end

      if deps.empty?
        depsname = "NULL"
      else
        depsname = "DEPSREF(deps_#{cname})"
      end

      no = "/* %3d */" % i

      %(#{no} { #{name.inspect}, #{init}, #{final}, #{deps.size}, #{depsname} })
    }.join(",\n  ")
  }
};

#{
  gems.map { |name, cname, gem, deps|
    if deps.empty?
      nil
    else
      deps = deps.map { |d| gindex[d] }.sort
      %(DEPSDEF(deps_#{cname}, #{deps.size}, #{deps.map { |d| %(&mgems_list[#{d}]) }.join(", ") }))
    end
  }.compact.join("\n")
}
    DEPS_H
  end
end
