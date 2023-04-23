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
  output1.gsub!(/(?:, "mruby-error")|(?:"mruby-error", )/, "") # for ...mruby-3.0.0
  output1.gsub!(/, "mruby-enum-ext"(?:, "mruby-enumerator")?(?:, "mruby-fiber")?/, "") # for ...2.1.0

  assert_equal <<-OUTPUT, output1
>> loaded gems: []
raised exceptions - undefined method 'puts' (NoMethodError)
>> loaded gems: ["mruby-print"]
b
>> loaded gems: ["mruby-print"]
raised exceptions - undefined method '%' (NoMethodError)
>> loaded gems: ["mruby-print", "mruby-sprintf"]
d
>> loaded gems: ["mruby-print", "mruby-sprintf"]
raised exceptions - uninitialized constant Math (NameError)
>> loaded gems: ["mruby-math"]
raised exceptions - undefined method 'p' (NoMethodError)
>> loaded gems: ["mruby-math", "mruby-print"]
-0.958924274663138
>> loaded gems: ["mruby-gemcut", "mruby-print"]
["mruby-gemcut", "mruby-print"]
>> loaded gems: ["mruby-gemcut", "mruby-math", "mruby-print", "mruby-sprintf"]
["mruby-gemcut", "mruby-math", "mruby-print", "mruby-sprintf"]
>> loaded gems: ["mruby-gemcut", "mruby-math", "mruby-print"]
{"mruby-math"=>true}
>> loaded gems: ["mruby-gemcut", "mruby-math", "mruby-print"]
["mruby-gemcut", "mruby-math", "mruby-print", "mruby-sprintf"]
>> loaded gems: ["mruby-gemcut", "mruby-math", "mruby-print", "mruby-sprintf"]
{"mruby-string-ext"=>false}
>> loaded gems: []
raised exceptions - uninitialized constant Gemcut (NameError)
>> loaded gems: ["mruby-gemcut"]
["mruby-gemcut", "mruby-print"]
>> loaded gems: ["mruby-gemcut"]
["mruby-array-ext", "mruby-gemcut", "mruby-hash-ext", "mruby-print"]
  OUTPUT
end
