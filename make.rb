require "optparse"
require "fileutils"
require "rbconfig"

options = {}

OptionParser.new do |opts|
  opts.on("-t", "--tests", "Run tests") do
    options[:tests] = true
  end

  opts.on("-c", "--clean", "Clear the build before build") do
    options[:clean] = true
  end

  opts.on("-i", "--install", "Install ASL") do
    options[:install] = true
  end

  opts.on("-g", "--debug", "Debug build") do
    options[:debug] = true
  end

  opts.on("-a", "--asan", "Address/Undefined Sanitizer") do
    options[:asan] = true
  end

  opts.on("-p", "--prefix PREFIX", "Sets the installation prefix") do |prefix|
    options[:prefix] = prefix
  end

  opts.on("-h", "--help", "Shows help") do
    puts opts
    exit 1
  end
end.parse!

def run(*cmd)
  puts "$ #{cmd.join(" ")}"
  system(*cmd) or abort "#{cmd.join(" ")} failed"
end

if RbConfig::CONFIG["host_os"] =~ /android/i
  options[:prefix] ||= ENV["PREFIX"] || ENV["HOME"] || "./"
end

build_dir = ".build"

if !File.exist?(build_dir) || options[:clean]
  FileUtils.rm_rf(build_dir)

  cmake_args = [
    "-S", ".",
    "-B", build_dir,
    "-G", "Ninja"
  ]

  if options[:prefix]
    cmake_args << "-DCMAKE_INSTALL_PREFIX=#{options[:prefix]}"
  end

  if options[:debug]
    cmake_args << "-DCMAKE_BUILD_TYPE=Debug"
  else
    cmake_args << "-DCMAKE_BUILD_TYPE=Release"
  end

  if options[:asan]
    cmake_args << "-DCMAKE_C_FLAGS=-fsanitize=address,undefined"
    cmake_args << "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined"
  end

  run "cmake", *cmake_args
end

compile_commands = "#{build_dir}/compile_commands.json"
FileUtils.cp(compile_commands, ".") if File.exist?(compile_commands)

run "cmake", "--build", build_dir

if options[:install]
  run "cmake", "--install", build_dir
end

if options[:tests]
  if options[:install]
    run "kpp", "compile", "examples/hello_world"
  else
    run "#{build_dir}/kpp", "compile", "examples/hello_world"
  end
end
