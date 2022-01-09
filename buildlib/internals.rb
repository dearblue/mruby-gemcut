require "mruby/source"

module Gemcut
  def Gemcut.need_error_gem?
    MRuby::Source::MRUBY_RELEASE_NO < 30100 and !File.exist?(File.join(MRUBY_ROOT, "mrbgems/mruby-binding"))
  end
end
