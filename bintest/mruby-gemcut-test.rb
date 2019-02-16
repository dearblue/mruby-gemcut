#$: << File.join(Dir.pwd, "lib")
#require "mruby/source"

assert "mruby-gemcut" do
  output = `#{cmd('mruby-gemcut-test')} 2>&1`
  output1 = output.dup
  output1.gsub!(/^(raised exceptions - [\S]+:)(?! )/, "\\1 ") # for mruby-1.3
  output1.gsub!(/^(raised exceptions - [\S]+: undefined method [\S]+) for [\S]+$/, "\\1") # for mruby-1.2, 1.3
  output1.gsub!(/^-0\.95892427466314$/, "-0.9589242746631385") # for mruby-1.2, 1.3, 1.4, 1.4.1

  assert_equal <<-OUTPUT, output1
>> committed gems: []
raised exceptions - NoMethodError: undefined method 'puts'
>> committed gems: ["mruby-print"]
b
>> committed gems: ["mruby-print"]
raised exceptions - NoMethodError: undefined method '%'
>> committed gems: ["mruby-print", "mruby-sprintf"]
d
>> committed gems: ["mruby-print", "mruby-sprintf"]
raised exceptions - NameError: uninitialized constant Math
>> committed gems: ["mruby-math"]
raised exceptions - NoMethodError: undefined method 'p'
>> committed gems: ["mruby-math", "mruby-print"]
-0.9589242746631385
>> committed gems: nil
raised exceptions - NoMethodError: undefined method 'puts'
>> committed gems: nil
raised exceptions - NoMethodError: undefined method 'puts'
>> committed gems: nil
>> committed gems: nil
["mruby-print"]
>> committed gems: nil
"mruby-math"
>> committed gems: nil
{"mruby-math"=>false}
>> committed gems: nil
{"mruby-io"=>nil}
>> committed gems: nil
["mruby-print"]
>> committed gems: nil
["mruby-test", "mruby-bin-mrbc", "mruby-compiler", "mruby-math", "mruby-print", "mruby-sprintf", "mruby-gemcut", "mruby-error"]
  OUTPUT
end
