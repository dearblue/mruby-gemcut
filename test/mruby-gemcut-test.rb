assert "without Gemcut module" do
  assert_equal %(undefined method 'sprintf' (NoMethodError)) do
    ret = gemcut_load_string false, "sprintf '%s', 'a'"
    ret.sub!("for Object ", "") # for mruby-3.4.0...
    ret
  end
  assert_equal %("b") do
    gemcut_load_string false, "sprintf '%s', 'b'", "mruby-sprintf"
  end
  assert_equal %(uninitialized constant Fiber (NameError)) do
    gemcut_load_string false, "Fiber.new { 5 }.resume", "mruby-sprintf"
  end
  assert_equal %(5) do
    gemcut_load_string false, "Fiber.new { 5 }.resume", "mruby-fiber"
  end
end

assert "with Gemcut module" do
  assert_equal %(["mruby-gemcut"]) do
    gemcut_load_string true, "Gemcut.loaded_features.sort"
  end
  assert_equal %(["mruby-gemcut", "mruby-sprintf"]) do
    gemcut_load_string true, "Gemcut.loaded_features.sort", "mruby-sprintf"
  end
  assert_equal %(["mruby-fiber", "mruby-gemcut", "mruby-sprintf"]) do
    gemcut_load_string true, "Gemcut.loaded_features.sort", "mruby-sprintf", "mruby-fiber"
  end
  assert_equal %(true) do
    gemcut_load_string true, "Gemcut.loaded_feature?('mruby-fiber')", "mruby-fiber"
  end
  assert_equal %(["mruby-fiber", "mruby-gemcut", "mruby-sprintf"]) do
    gemcut_load_string true, "Gemcut.require 'mruby-sprintf'; Gemcut.loaded_features.sort", "mruby-fiber"
  end
  assert_equal %(false) do
    gemcut_load_string true, "Gemcut.loaded_feature?('mruby-string-ext')", "mruby-sprintf", "mruby-fiber"
  end
end

#  gemcut_load_string FALSE, "Gemcut.require 'mruby-gemcut'; Gemcut.require 'mruby-sprintf'; Gemcut.loaded_features.sort"
#  gemcut_load_string TRUE, "Gemcut.require 'mruby-gemcut'; Gemcut.require 'mruby-sprintf'; Gemcut.loaded_features.sort"
#  gemcut_load_string TRUE, "Gemcut.require 'mruby-hash-ext'; Gemcut.require 'mruby-sprintf'; Gemcut.loaded_features.sort"
