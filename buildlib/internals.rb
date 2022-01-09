require "mruby/source"

module Gemcut
  def Gemcut.need_error_gem?
    MRuby::Source::MRUBY_RELEASE_NO < 30100 and !File.exist?(File.join(MRUBY_ROOT, "mrbgems/mruby-binding"))
  end
end

require "fileutils"
require "pathname"

module Gemcut
  Entry = Struct.new(*%i(name cname gem deps))
  Model = Struct.new(*%i(name bundle pass drop backtrace))

  module Internals
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

      def build_pass_list_for(listname, gems, gindex, bundle, common_drop, backtrace)
        gems.size.times.to_a - bundle - common_drop
      end

      def build_drop_list_for(listname, gems, gindex, bundle, backtrace)
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

      def build_pass_list_for(listname, gems, gindex, bundle, common_drop, backtrace)
        []
      end

      def build_drop_list_for(listname, gems, gindex, bundle, backtrace)
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
    end

    refine Array do
      def build_pass_list_for(listname, gems, gindex, bundle, common_drop, backtrace)
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

      def build_drop_list_for(listname, gems, gindex, bundle, backtrace)
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
  end
end
