begin
  require "mruby/source"
rescue LoadError
  $: << File.join(MRUBY_ROOT, "lib")
  require "mruby/source"
end

require "fileutils"
require "io/console"
require "pathname"

desc "generate dependency files for mruby-gemcut"
task "check-mruby-gemcut" do
  puts "mruby-gemcut: good your design!"
end

testtask = (MRuby::Source::MRUBY_RELEASE_NO >= 30000) ? "test:run" : "test"

task testtask => "test-mruby-gemcut-config"

task "test-mruby-gemcut-config" do
  puts ">>> configuration test for mruby-gemcut <<<"
  env = { "MRUBY_ROOT" => MRUBY_ROOT }
  system env, "ruby", File.join(File.dirname(__dir__), "testgem/conftest/conftest.rb") or fail
end

module Gemcut
  def Gemcut.need_error_gem?
    MRuby::Source::MRUBY_RELEASE_NO < 30100
  end

  Entry = Struct.new(*%i(name cname gem deps))
  Model = Struct.new(*%i(name bundle allow deny backtrace))

  module Internals
    if Object.const_defined?(:MiniRake)
      refine MiniRake::Task do
        attr_accessor :actions
      end
    end

    refine Numeric do
      def decide_inttype
        case
        when self <   256; "uint8_t"
        when self < 65536; "uint16_t"
        else               "uint32_t"
        end
      end
    end

    refine Object do
      def ensure_array_or_nil
        [] | self
      rescue TypeError
        $!.set_backtrace caller(2)
        raise $!
      end

      def ensure_array_or_state
        [] | self
      rescue TypeError
        $!.set_backtrace caller(2)
        raise $!
      end

      def ensure_string
        "" + self
      rescue TypeError
        $!.set_backtrace caller(2)
        raise $!
      end
    end

    refine TrueClass do
      def empty?
        false
      end

      def ensure_array_or_state
        self
      end

      def build_allow_list_for(listname, gems, gindex, bundle, common_deny, backtrace)
        gems.size.times.to_a - bundle - common_deny
      end

      def build_deny_list_for(listname, gems, gindex, bundle, backtrace)
        gems.size.times.to_a - bundle
      end
    end

    refine FalseClass do
      def empty?
        false
      end

      def ensure_array_or_state
        self
      end

      def build_allow_list_for(listname, gems, gindex, bundle, common_deny, backtrace)
        []
      end

      def build_deny_list_for(listname, gems, gindex, bundle, backtrace)
        []
      end
    end

    refine NilClass do
      def empty?
        true
      end

      def ensure_array_or_nil
        []
      end

      def ensure_array_or_state
        []
      end

      def bytesize
        0
      end

      def b
        "".b
      end
    end

    refine Array do
      def build_allow_list_for(listname, gems, gindex, bundle, common_deny, backtrace)
        list = uniq
        list.compact!

        list.map! { |e| gindex[e] || raise(RuntimeError, %(not exist gem "#{e}" of #{listname}), backtrace) }

        queue = list.dup
        until queue.empty?
          no = queue.pop
          d = gems[no].deps - list - queue
          queue.concat d
          list.concat d
        end
        list.sort!
        list
      end

      def build_deny_list_for(listname, gems, gindex, bundle, backtrace)
        list = uniq
        list.compact!

        list.map! { |e| gindex[e] || $stderr.puts(%(warning: not exist gem "#{e}" of #{listname} - from #{backtrace[0]})) }
        list.compact!

        queue = list.dup
        until queue.empty?
          no = queue.pop
          d = gems.each_with_object([]).with_index { |(e, a), i| e.deps.include?(no) && (a << i) }
          d -= list - queue
          queue.concat d
          list.concat d
        end
        list.sort!
        list
      end

      def check_conflict_set(gems, other_list, err_mesg, backtrace)
        confl = self & other_list
        unless confl.empty?
          confl.map! { |e| gems[e].name }
          confl.sort!
          raise RuntimeError, %(#{err_mesg} - #{confl.join(", ")}), backtrace
        end
      end

      def make_bitmap(bitmap)
        each do |e|
          bitmap[e / 32] |= 1 << (e % 32)
        end
        bitmap
      end
    end

    refine MRuby::Gem::Specification do
      # mruby-gemcut/deps.h を生成するためのタスク
      def make_depsfile_task
        hdrgendir = File.join(build_dir, "include")
        deps_h = File.join(hdrgendir, "mruby-gemcut/deps.h")
        gemcut_o = File.join(build_dir, "src/mruby-gemcut.c").ext(exts.object)
        cc.include_paths << hdrgendir
        file gemcut_o => [File.join(dir, "src/mruby-gemcut.c"), deps_h]
        task "check-mruby-gemcut" => deps_h
        file deps_h => [__FILE__, File.join(build.build_dir, "mrbgems/gem_init.c")] do |t|
          # NOTE: file タスク中であれば build.gems はすでに依存関係が解決されている状態。

          verbose = Rake.respond_to?(:verbose) ? Rake.verbose : $-v
          puts %(GEN   #{t.name.relative_path}#{verbose ? " (by #{__FILE__.relative_path})" : nil}\n)

          gindex = build.gems.each_with_index.with_object({}) { |(g, i), a| a[g.name] = i }
          gems = build.gems.map do |g|
            name = "#{g.name}"
            cname = name.gsub(/[^0-9A-Za-z_]+/, "_")
            Gemcut::Entry[name, cname, g, g.dependencies.map { |e| gindex[e[:gem].to_s] }.sort]
          end

          gemcut_max_gems = 4000
          if gems.size > gemcut_max_gems
            raise "The allowable gem number in '#{s.name}' has been exceeded (maximum #{gemcut_max_gems})"
          end

          models = @models.dup
          common_deny = models[0].deny

          if build.bintest_enabled?
            default_bundles = {}
            traverse_deps = ->(d) { default_bundles[d] = true; gems[d].deps.each { |e| traverse_deps.call(e) } }
            gems.each_with_index do |g, i|
              bins = g.gem.bins&.flatten
              next if bins.nil? || bins.empty?
              traverse_deps.call i
            end
            models[0].bundle = models[0].bundle | default_bundles.each_key.map { |d| gems[d].name }
            (allows, denies) = models[0].bundle.sort.each_with_object([[], []]) { |e, (a, d)|
              if common_deny.include?(e)
                d << e
              else
                a << e
              end
            }

            unless denies.empty? && allows.empty?
              width = ($stderr.tty? ? IO.console.winsize[1] : 80) - 8.0

              (allows, denies) = [allows, denies].map { |c|
                next if c.empty?

                align = c.max_by { |e| e.bytesize }.bytesize + 2
                align1 = align - 1
                lines = [(c.sum { |e| e.bytesize + 2 } / width).ceil, 5].max # 行を強調するため、最低5行を確保する
                c.each_with_index.with_object([]) { |(e, i), a|
                  buf = a[i % lines] ||= "    "
                  buf << " " * (align1 - (buf.bytesize - 4 + align1) % align)
                  buf << e
                }
              }

              unless allows.empty?
                allows.unshift "Some gems are activated for bintest."
                allows.each { |c| c.insert 0, "  | "; c << "\n" }
              end

              unless denies.empty?
                denies.unshift "\e[1;3m(!!) Some gems are added by add_denylist, but activated for bintest.\e[m"
                denies.each { |c| c.insert 0, "  | "; c << "\n" }
              end

              warn <<~WARN
                \e[7mwarning from mruby-gemcut\e[m
                #{denies.empty? ? nil : denies.join}#{allows.empty? ? nil : allows.join}
              WARN
            end
            models[0].deny = common_deny - models[0].bundle
          end

          common_deny = common_deny.build_deny_list_for("common deny list", gems, gindex, [], models[0].backtrace)

          models.map! do |m|
            bundle = m.bundle.build_allow_list_for("bundle list", gems, gindex, [], [], m.backtrace)
            allow = m.allow.build_allow_list_for("allow list", gems, gindex, bundle, (m.name ? common_deny : []), m.backtrace)
            deny = m.deny.build_deny_list_for("deny list", gems, gindex, bundle, m.backtrace)
            deny |= common_deny if m.name
            bundle.check_conflict_set(gems, deny, %(incorrect GEM contained in both "bundle list" and "deny list"), m.backtrace)
            allow.check_conflict_set(gems, deny, %(incorrect GEM contained in both "allow list" and "deny list"), m.backtrace)
            allow = gems.size.times.to_a - deny if allow.empty?
            Gemcut::Model.new(m.name, bundle, allow, deny, m.backtrace)
          end

          unavailables = gems.reject.with_index { |g, i| models.find { |m| m.allow.include?(i) } }
          unless unavailables.empty?
            unavailables.sort_by { |g| g.name }.each do |g|
              $stderr.puts "\e[1mwarning: `#{g.name}` is not allowed from any model of mruby-gemcut.\e[m"
            end
          end

          unit_bits = 32

          FileUtils.mkpath File.dirname t.name
          File.write t.name, <<~"DEPS_H", mode: "wb"
            /*
             * This file is auto generated by mruby-gemcut.
             * THE CONTENT YOU CHANGED WILL BE LOST.
             */

            #define MRUBY_GEMCUT_ID #{gems.index { |g| g.name == "mruby-gemcut" }}
            #define MGEMS_POPULATION #{gems.size}
            #define MGEMS_BITMAP_UNITS #{gems.empty? ? 1 : (gems.size + (unit_bits - 1)) / unit_bits}
            #define MGEMS_UNIT_BITS #{unit_bits}

            typedef uint32_t bitmap_unit;
            typedef #{(gems.sum { |g| g.name.bytesize } + models.sum { |m| m.name.bytesize }).decide_inttype} gemcut_name_index_t;
            typedef #{gems.sum { |g| g.deps.size }.decide_inttype} gemcut_deps_index_t;
            typedef void init_final_f(mrb_state *);

            struct gemcut_model
            {
              bitmap_unit bundle[MGEMS_BITMAP_UNITS];
              bitmap_unit avail[MGEMS_BITMAP_UNITS];
              gemcut_name_index_t name_index_end;
            };

            struct mrbgem_spec
            {
              init_final_f *gem_init;
              init_final_f *gem_final;
              gemcut_name_index_t name_index_end;
              gemcut_deps_index_t deps_index_end;
            };

            #define MRUBY_GEMCUT_SPEC_FOREACH(DEF) \\
              /* (index)  (name index)  (name)  (cfunc)  (deps_index_end)  (dep_list) */ \\
              #{
                name_index_end = 0
                deps_index_end = 0
                gems.each_with_index.with_object("") { |(g, i), a|
                  a << "\n  " unless a.empty?
                  cfunc = g.gem.generate_functions ? g.cname : "NO_CFUNC"
                  name_size = g.name.bytesize
                  name_index_end += name_size
                  deps_index_end += g.deps.size
                  dep_list = g.deps.empty? ? "EMPTY" : %(EXPAND(#{g.deps.map { |d| %(#{d}) }.join(", ")}))
                  a << %(DEF(%3d, %4d, %s, %s, %s, %s) \\) % [i, name_index_end, g.name.inspect, cfunc, deps_index_end, dep_list]

                  if i == 0 && deps_index_end != 0
                    raise "the end of the dependency index for the first element should indicate 0"
                  end
                }
              }

            #define MRUBY_GEMCUT_MODEL_FOREACH(DEF) \\
              /* (index)  (name index)  (name)  (bundle)  (available) */ \\
              #{
                models.each_with_index.with_object("") { |(m, i), a|
                  a << "\n  " unless a.empty?
                  bundle = m.bundle.make_bitmap([0] * ((gems.size + 31) / 32))
                  allow = m.allow.make_bitmap(bundle.dup)
                  bundle = bundle.map { |e| "0x%08xUL" % e }.join(", ")
                  allow = allow.map { |e| "0x%08xUL" % e }.join(", ")
                  name = m.name.b
                  name_index_end += name.bytesize
                  a << %(DEF(%3d, %4d, %s, (%s), (%s)) \\) % [i, name_index_end, name.inspect, bundle, allow]
                }
              }

            #define MAKE_GEMFUNC_PAIR(CNAME) GENERATED_TMP_mrb_ ## CNAME ## _gem_init, GENERATED_TMP_mrb_ ## CNAME ## _gem_final
            #define MRUBY_GEMCUT_EXPAND(...) __VA_ARGS__,
            #define MRUBY_GEMCUT_EMPTY

            #define MRUBY_GEMCUT_FUNC_DECLS(I, A, N, F, D, L) init_final_f MAKE_GEMFUNC_PAIR(F);
            MRUBY_GEMCUT_SPEC_FOREACH(MRUBY_GEMCUT_FUNC_DECLS)

            #define GENERATED_TMP_mrb_NO_CFUNC_gem_init  NULL
            #define GENERATED_TMP_mrb_NO_CFUNC_gem_final NULL
            #define MRUBY_GEMCUT_SPEC_DECLS(I, A, N, F, D, L) { MAKE_GEMFUNC_PAIR(F), A, D },
            static const struct mrbgem_spec mrbgems_list[] = {
              MRUBY_GEMCUT_SPEC_FOREACH(MRUBY_GEMCUT_SPEC_DECLS)
            };

            #define MRUBY_GEMCUT_DEPENDS_JOIN(T, L) T ## L
            #define MRUBY_GEMCUT_DEPENDS_DECLS(I, A, N, F, D, L) MRUBY_GEMCUT_ ## L
            static const gemcut_deps_index_t mrbgems_deps_list[] = {
              MRUBY_GEMCUT_SPEC_FOREACH(MRUBY_GEMCUT_DEPENDS_DECLS)
            };

            #define MRUBY_GEMCUT_MODEL_DECLS(I, A, N, B, V) { { MRUBY_GEMCUT_EXPAND B }, { MRUBY_GEMCUT_EXPAND V }, A },
            static const struct gemcut_model gemcut_models[] = {
              MRUBY_GEMCUT_MODEL_FOREACH(MRUBY_GEMCUT_MODEL_DECLS)
            };

            #define MRUBY_GEMCUT_GEMNAME_DECLS(I, A, N, F, D, L) N
            #define MRUBY_GEMCUT_MODELNAME_DECLS(I, A, N, B, V) N
            static const char gemcut_name_table[] = {
              MRUBY_GEMCUT_SPEC_FOREACH(MRUBY_GEMCUT_GEMNAME_DECLS)
              MRUBY_GEMCUT_MODEL_FOREACH(MRUBY_GEMCUT_MODELNAME_DECLS)
            };
          DEPS_H
        end
      end

      def make_geminit_task
        file "#{build.build_dir}/mrbgems/gem_init.c" => [__FILE__] do |t|
          t.actions[1..-1] = []
          FileUtils.mkpath File.dirname(t.name)
          File.binwrite t.name, <<~CODE
            /*
             * The content generated by mruby has been replaced by mruby-gemcut.
             * THE CONTENT YOU CHANGED WILL BE LOST.
             */

            #include <mruby.h>
            #include "#{self.dir}/include/mruby-gemcut.h"

            void
            mrb_init_mrbgems(mrb_state *mrb)
            {
              mruby_gemcut_model_select(mrb, NULL);
            }
          CODE
        end
      end
    end
  end
end
