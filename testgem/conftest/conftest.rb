GEMNAME ||= "mruby-gemcut"

require File.join(ENV["MRUBY_ROOT"], "test/assert")
require "fileutils"
require "tmpdir"

tmpdir = Dir.mktmpdir("mruby-gemcut-test")
END { FileUtils.rm_rf tmpdir }

USE_RAKE = ENV["RAKE"] || "rake"

using Module.new {
  refine Object do
    define_method(:open_tempfile, ->(path, *args, &block) { File.open(File.join(tmpdir, path), *args, &block) })

    count = 0
    define_method(:auto_increment, -> { c = count; count += 1; c })

    def assert_build_gemcut_with_fail(pat, confpart)
      conf = open_tempfile("test_config#{auto_increment}.rb", "wb")
      builddir = File.join(__dir__, "build", File.basename(conf, ".*"))
      conf.puts <<~CONFIG
        MRuby::Lockfile.disable rescue nil

        MRuby::Build.new do
          toolchain "gcc"
          self.build_dir = #{builddir.inspect}
          #{confpart}
        end
      CONFIG
      conf.close

      env = { "MRUBY_CONFIG" => conf.to_path, "INSTALL_DIR" => File.join(builddir, "installbin") }
      result = IO.popen(env, %W(#{USE_RAKE} check-mruby-gemcut), "rb", err: [:child, :out]) { |io| io.read }

      assert_true(!!(pat =~ result), %(not matched with #{pat.inspect}), result)
    end
  end
}

assert "mruby-gemcut when building" do
  assert_build_gemcut_with_fail(/^TypeError: no implicit conversion of Integer into Array\b/, <<~CONFIG_PART)
    gembox "default"
    gem %(#{File.dirname File.dirname __dir__}) do |g|
      g.add_model "default", bundle: 9
    end
  CONFIG_PART

  assert_build_gemcut_with_fail(/^TypeError: no implicit conversion of Integer into Array\b/, <<~CONFIG_PART)
    gembox "default"
    gem %(#{File.dirname File.dirname __dir__}) do |g|
      g.add_model "default", allow: 9
    end
  CONFIG_PART

  assert_build_gemcut_with_fail(/^TypeError: no implicit conversion of Integer into Array\b/, <<~CONFIG_PART)
    gembox "default"
    gem %(#{File.dirname File.dirname __dir__}) do |g|
      g.add_model "default", deny: 9
    end
  CONFIG_PART

  assert_build_gemcut_with_fail(/^ArgumentError: the `allow` and `deny` arguments are exclusive\b/, <<~CONFIG_PART)
    gembox "default"
    gem %(#{File.dirname File.dirname __dir__}) do |g|
      g.add_model "default", allow: true, deny: true
    end
  CONFIG_PART

  assert_build_gemcut_with_fail(/^incorrect GEM contained in both "bundle list" and "deny list" - mruby-bin-mruby$/, <<~CONFIG_PART)
    gembox "default"
    gem %(#{File.dirname File.dirname __dir__}) do |g|
      g.add_denylist "mruby-bin-mruby", "mruby-bin-mirb"
      g.add_model "default", bundle: %w(mruby-bin-mruby)
    end
  CONFIG_PART

  assert_build_gemcut_with_fail(/^incorrect GEM contained in both "bundle list" and "deny list" - mruby-enumerator, mruby-fiber$/, <<~CONFIG_PART)
    gembox "default"
    gem %(#{File.dirname File.dirname __dir__}) do |g|
      g.add_model "default", bundle: %w(mruby-print mruby-enumerator), deny: %w(mruby-fiber)
    end
  CONFIG_PART
end

exit (report || 0)
