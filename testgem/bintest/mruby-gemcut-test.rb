#$: << File.join(Dir.pwd, "lib")
#require "mruby/source"

assert "mruby-gemcut" do
  output = `#{cmd('mruby-gemcut-test')} 2>&1`
  output1 = output.dup
  output1.gsub!(/^(raised exceptions - [\S]+:)(?! )/, "\\1 ") # for mruby-1.3
  output1.gsub!(/^(raised exceptions - [\S]+: undefined method [\S]+) for [\S]+$/, "\\1") # for mruby-1.2, 1.3
  output1.gsub!(/^-0\.95892427466314$/, "-0.9589242746631385") # for mruby-1.2, 1.3, 1.4, 1.4.1
  output1.gsub!(/(?<= - )(?:(\w+Error): )(.*)/, "\\2 (\\1)") # for ..mruby-3.0.0
  output1.gsub!(/^-0\.9589242746631385$/, "-0.958924274663138") # for ..mruby-3.0.0
  output1.gsub!(', "mruby-test"', "") # for ...mruby-3.0.0

  assert_equal <<-OUTPUT, output1
>> committed gems: []
raised exceptions - undefined method 'puts' (NoMethodError)
>> committed gems: ["mruby-print"]
b
>> committed gems: ["mruby-print"]
raised exceptions - undefined method '%' (NoMethodError)
>> committed gems: ["mruby-print", "mruby-sprintf"]
d
>> committed gems: ["mruby-print", "mruby-sprintf"]
raised exceptions - uninitialized constant Math (NameError)
>> committed gems: ["mruby-math"]
raised exceptions - undefined method 'p' (NoMethodError)
>> committed gems: ["mruby-math", "mruby-print"]
-0.958924274663138
>> committed gems: nil
raised exceptions - undefined method 'puts' (NoMethodError)
>> committed gems: nil
raised exceptions - undefined method 'puts' (NoMethodError)
>> committed gems: nil
>> committed gems: nil
["mruby-print"]
>> committed gems: nil
["mruby-math", "mruby-print", "mruby-sprintf"]
>> committed gems: nil
{"mruby-math"=>true}
>> committed gems: nil
{"mruby-string-ext"=>false}
>> committed gems: nil
["mruby-print"]
  OUTPUT
end
