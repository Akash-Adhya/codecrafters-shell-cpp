#include <cmath>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <ranges>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
uint64_t toInt(const std::string& s) {
  uint64_t o{0};
  uint8_t i{0};
  for (const char& c : s | std::views::reverse) {
    switch (c) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        o += pow(10, i) * (c - 48);
        break;
      default: return 0;
    }
  }
  return o;
}
static inline uint64_t echo(const std::vector<std::string>& arguments) {
  switch (arguments.size()) {
    case 1: goto end;
    default: std::cout << arguments[1];
  }
  for (const std::string& a : arguments | std::views::drop(2)) {
    std::cout << " " << a;
  }
  end:
  std::cout << std::endl;
  return 0;
}
extern char** environ;
static std::map<std::string, std::string> ENV{};
constexpr const std::vector<std::filesystem::path> PATH() {
  std::vector<std::filesystem::path> ret;
  if (!ENV.contains("PATH")) return ret;
  std::string path;
  for (const char& c : ENV["PATH"]) {
    if (c == ':') {
      ret.emplace_back(path);
      path = "";
      continue;
    }
    path += c;
  }
  ret.emplace_back(path);
  return ret;
}
constexpr const std::filesystem::path PWD() {
  if (!ENV.contains("PWD")) return {};
  return ENV["PWD"];
}
constexpr const std::filesystem::path HOME() {
  if (!ENV.contains("HOME")) return "/";
  return ENV["HOME"];
}
uint64_t pwd() {
  std::cout << PWD().string() << std::endl;
  return 0;
}
uint64_t cd(const std::vector<std::string>& arguments) {
  std::string arg;
  if (arguments.size() == 1) arg = "~";
  else if (arguments.size() == 2) arg = arguments[1];
  else {
    std::cout << "cd: too many arguments" << std::endl;
    return 1;
  }
  if (!arg.size()) return 0;
  if (arg.front() == '~') arg.replace(0, 1, HOME());
  std::filesystem::path p{arg};
  if (p.is_relative()) p = PWD() / p;
  if (!std::filesystem::exists(p)) {
    std::cout
      << "cd: "
      << p.string()
      << ": No such file or directory"
      << std::endl;
    return 1;
  }
  if (!std::filesystem::is_directory(p)) {
    std::cout << "cd: " << p.string() << ": Not a directory" << std::endl;
    return 1;
  }
  p = p.lexically_normal();
  // Remove trailing slash unless at the root.
  if (!p.has_filename() && p.has_parent_path()) p = p.parent_path();
  ENV["PWD"] = p;
  return 0;
}
std::optional<std::filesystem::path> locateBinary(const std::string& name) {
  if (name == "") return std::nullopt;
  using namespace std::filesystem;
  for (const path& d : PATH()) {
    if (!exists(d)) continue;
    for (const directory_entry& f : directory_iterator(d)) {
      if (f.path().filename() == name) return f.path();
    }
  }
  return std::nullopt;
}
static constexpr std::array<std::string_view, 5> BUILTINS{
  "cd",
  "exit",
  "echo",
  "pwd",
  "type"
};
inline uint64_t type(const std::vector<std::string>& arguments) {
  if (arguments.size() == 1) return 0;
  uint64_t ret{0};
  for (const std::string& a : arguments | std::views::drop(1)) {
    for (const std::string_view& b : BUILTINS) {
      if (a == b) {
        std::cout << a << " is a shell builtin" << std::endl;
        goto found;
      }
    }
    {
      const std::optional<std::filesystem::path> b{locateBinary(a)};
      if (b.has_value()) {
        std::cout << a << " is " << b.value().string() << std::endl;
      } else {
        std::cout << a << ": not found" << std::endl;
        ret = 1;
      }
    }
    found:
  }
  return ret;
}
uint64_t waitForExitStatus(const pid_t& pid) {
  int status;
  waitpid(pid, &status, WUNTRACED);
  // What about WCONTINUED?  Bro I dunno lmao.
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return -1;
}
uint64_t run(
  const std::filesystem::path& binary,
  const std::vector<std::string>& arguments
) {
  const pid_t pid{fork()};
  if (pid == 0) {
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const std::string& a : arguments) {
      argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(NULL);
    std::vector<char*> envp;
    envp.reserve(ENV.size() + 1);
    for (const auto& [name, value] : ENV) {
      envp.push_back(const_cast<char*>((name + '=' + value).c_str()));
    }
    envp.push_back(NULL);
    execve(binary.string().c_str(), &argv[0], &envp[0]);
    std::cout << std::strerror(errno) << std::endl;
    exit(-1);
  }
  else if (pid < 0) return pid;
  else return waitForExitStatus(pid);
}
void consumeEnv() {
  std::string name;
  std::string value;
  bool gotName;
  for (char** e = environ; *e; e++) {
    name = "";
    value = "";
    gotName = false;
    for (char* c = *e; *c; c++) {
      if (!gotName) {
        if (*c == '=') gotName = true;
        else name += *c;
      } else {
        value += *c;
      }
    }
    ENV[name] = value;
  }
}
int main() {
  consumeEnv();
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  std::string input;
  std::vector<std::string> arguments;
  
  while (true) {
    std::cout << "$ ";
    std::getline(std::cin, input);
    
    // Reset parsing variables for each new input line.
    arguments.clear();
    std::string word;
    bool singleQuoted = false;
    bool doubleQuoted = false;
    bool escaped = false;
    bool concatNext = false;  // New flag for merging adjacent quoted strings
    
    for (const char& c : input) {
      if (escaped) {
        word += c;
        escaped = false;
      }
      else if (c == '\\' && !singleQuoted) {
        escaped = true;
      }
      else if (c == ' ' && !singleQuoted && !doubleQuoted) {
        if (!word.empty()) {
          if (concatNext && !arguments.empty()) {
            arguments.back() += word; // Merge with previous argument
          } else {
            arguments.emplace_back(word);
          }
          word.clear();
          concatNext = false;
        }
      }
      else if (c == '\'' && !doubleQuoted) {
        singleQuoted = !singleQuoted;
      }
      else if (c == '"' && !singleQuoted) {
        if (doubleQuoted) {
          // Closing a double quote, mark for concatenation.
          concatNext = true;
          doubleQuoted = false;
        } else {
          doubleQuoted = true;
        }
      }
      else {
        word += c;
      }
    }
    
    // If there's a remaining word, add or merge it.
    if (!word.empty()) {
      if (concatNext && !arguments.empty()) {
        arguments.back() += word;
      } else {
        arguments.emplace_back(word);
      }
    }
    
    if (arguments.empty()) continue;
    
    // Process commands as before.
    if (arguments[0] == "exit") {
      switch (arguments.size()) {
        case 1: return 0;
        case 2: return toInt(arguments[1]);
        default: std::cout << arguments[0] << ": too many arguments";
      }
    }
    else if (arguments[0] == "echo") echo(arguments);
    else if (arguments[0] == "type") type(arguments);
    else if (arguments[0] == "pwd") pwd();
    else if (const auto& b{locateBinary(arguments[0])}; b.has_value()) {
      run(b.value(), arguments);
    }
    else if (arguments[0] == "cd") cd(arguments);
    else std::cout << input << ": not found" << std::endl;
  }
}
