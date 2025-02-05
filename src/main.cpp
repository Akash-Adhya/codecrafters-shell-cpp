#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <fstream>
#include <sstream>

#define UNIMPLEMENTED(msg) do { std::cerr << __FILE__ << ":" << __LINE__ << ": UNIMPLEMENTED: " << msg << std::endl; exit(1); } while (false)
#define UNREACHABLE(msg) do { std::cerr << __FILE__ << ":" << __LINE__ << ": UNREACHABLE" << std::endl; exit(1); } while (false)

enum class QuoteMode {
    UNQUOTED,
    SINGLE,
    DOUBLE,
};

struct StringArray {
    std::vector<std::string> data;

    void add(const std::string& value) {
        data.push_back(value);
    }

    void clear() {
        data.clear();
    }
};

struct Command {
    std::string command;
    std::string description;
    int (*function)(StringArray& args);
};

std::vector<Command> builtins;
std::vector<FILE*> files;

void close_open_files() {
    for (auto& file : files) {
        if (file != nullptr) {
            fclose(file);
            file = nullptr;
        }
    }
}

std::string _read_arg(const std::string& string, const char* delim, size_t* rest, bool* quoted, QuoteMode* quote, bool* cont) {
    *cont = false;
    std::string ret;
    const char* p = string.c_str() + *rest;

    while (*p != '\0' && (*quote != QuoteMode::UNQUOTED || strchr(delim, *p) == nullptr)) {
        if (*quote == QuoteMode::UNQUOTED && *p == '>') {
            if (p == string.c_str()) {
                ret.push_back('>');
                p++;
                if (*p == '>') {
                    ret.push_back('>');
                    p++;
                }
            }
            *rest = p - string.c_str();
            ret.push_back('\0');
            return ret;
        }

        switch (*p) {
            case '\\': {
                switch (*quote) {
                    case QuoteMode::DOUBLE: {
                        p++;
                        switch (*p) {
                            case '\\':
                            case '$':
                            case '"':
                            case '\n':
                                ret.push_back(*p);
                                break;
                            default:
                                ret.push_back('\\');
                                ret.push_back(*p);
                                break;
                        }
                    }; break;

                    case QuoteMode::SINGLE:
                        ret.push_back(*p);
                        break;

                    case QuoteMode::UNQUOTED:
                        p++;
                        switch (*p) {
                            case '\n':
                                assert(*(p + 1) == '\0');
                                *cont = true;
                                break;
                            default:
                                ret.push_back(*p);
                                break;
                        }
                        break;

                    default:
                        UNREACHABLE("Unknown quote mode");
                        break;
                }
            }; break;

            case '"': {
                switch (*quote) {
                    case QuoteMode::UNQUOTED:
                        *quote = QuoteMode::DOUBLE;
                        *quoted = true;
                        break;

                    case QuoteMode::SINGLE:
                        ret.push_back('"');
                        break;

                    case QuoteMode::DOUBLE:
                        *quote = QuoteMode::UNQUOTED;
                        break;

                    default:
                        UNREACHABLE("Unknown quote mode");
                        break;
                }
            }; break;

            case '\'': {
                switch (*quote) {
                    case QuoteMode::UNQUOTED:
                        *quote = QuoteMode::SINGLE;
                        *quoted = true;
                        break;

                    case QuoteMode::SINGLE:
                        *quote = QuoteMode::UNQUOTED;
                        break;

                    case QuoteMode::DOUBLE:
                        ret.push_back('\'');
                        break;

                    default:
                        UNREACHABLE("Unknown quote mode");
                        break;
                }
            }; break;

            default:
                ret.push_back(*p);
                break;
        }
        p++;
    }

    if (*quote != QuoteMode::UNQUOTED) {
        assert(*p == '\0');
        *cont = true;
    }
    *rest = p - string.c_str();
    ret.push_back('\0');
    return ret;
}

int run_program(const std::string& file_path, StringArray& args) {
    std::vector<char*> argv;
    for (const auto& arg : args.data) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    switch (pid) {
        case -1:
            assert(false && "Out of memory");
            UNREACHABLE("Fork failed");
            return -1;

        case 0: {
            for (size_t i = 0; i < files.size(); i++) {
                if (files[i] != nullptr) {
                    int fd = fileno(files[i]);
                    assert(dup2(fd, i) == i);
                }
            }
            assert(execve(file_path.c_str(), argv.data(), environ) != -1);
            UNREACHABLE("Exec failed");
            return -1;
        }

        default: {
            int wstatus = 0;
            assert(waitpid(pid, &wstatus, 0) != -1);
            return WEXITSTATUS(wstatus);
        }
    }

    return -1;
}

std::string continue_arg(std::string existing, std::string string, const char* delim, size_t* rest, bool* quoted, QuoteMode* quote, bool* cont) {
    assert(*cont);

    size_t start_idx = *rest;
    const char* start = string.c_str() + start_idx;

    // Skip over delimiters if not quoted
    if (*quote == QuoteMode::UNQUOTED) {
        while (*start != '\0' && strchr(delim, *start) != nullptr) {
            start++;
        }
    }

    if (*start == '\0' || *start == '\n') {
        // If end of string or newline, return the existing string
        if (*quote == QuoteMode::UNQUOTED) return existing;

        size_t existing_len = existing.length();
        size_t arg_len = strlen(start);

        assert(arg_len <= 1); // Expected a single line from fgets

        // Append the current string to the existing one
        existing += std::string(start, arg_len);
        return existing;
    }

    // Read the next argument
    std::string arg = _read_arg(start, delim, rest, quoted, quote, cont);

    if (arg.empty()) return existing; // If no argument, return existing

    size_t existing_len = existing.length();
    size_t arg_len = arg.length();

    // Append the argument to the existing string
    existing += arg;

    return existing;
}

int help_command(StringArray& args) {
    FILE* out = stdout;
    if (files.size() > STDOUT_FILENO && files[STDOUT_FILENO] != nullptr) {
        out = files[STDOUT_FILENO];
    }

    FILE* err = stderr;
    if (files.size() > STDERR_FILENO && files[STDERR_FILENO] != nullptr) {
        err = files[STDERR_FILENO];
    }

    if (args.data.size() > 1) {
        std::string arg = args.data[1];
        for (const auto& cmd : builtins) {
            if (cmd.command == arg) {
                fprintf(out, "    %-10s - %s\n", cmd.command.c_str(), cmd.description.c_str());
                return 0;
            }
        }
        fprintf(err, "%s: Builtin %s not found\n", args.data[0].c_str(), args.data[1].c_str());
        return 1;
    }
    fprintf(out, "Available commands:\n");
    for (const auto& cmd : builtins) {
        fprintf(out, "    %-10s - %s\n", cmd.command.c_str(), cmd.description.c_str());
    }
    return 0;
}

// Implement other commands like `exit_command`, `echo_command`, `pwd_command`, etc. in a similar manner...

int main(int argc, char** argv) {
    builtins.push_back({"help", "Displays help about commands.", help_command});
    // Add other built-in commands...

    // Flush after every printf
    setbuf(stdout, nullptr);

    while (!feof(stdin)) {
        // FIXME read PS1
        std::cout << "$ ";

        // Wait for user input
        char input[100];
        if (fgets(input, 100, stdin) == nullptr) break;

        const char* delim = " \n";
        StringArray args;
        bool error = false;
        QuoteMode quote = QuoteMode::UNQUOTED;
        size_t rest = 0;
        std::string arg;
        bool quoted;
        bool get_new_line = false;

        while (!(arg = _read_arg(input, delim, &rest, &quoted, &quote, &get_new_line)).empty()) {
            while (get_new_line) {
                assert(rest == 0 || input[rest] == '\0');
                // FIXME read PS2
                std::cout << "> ";

                // Wait for user input
                if (fgets(input, 100, stdin) == nullptr) {
                    switch (quote) {
                        case QuoteMode::SINGLE:
                            std::cerr << "syntax error: Unexpected EOF while looking for matching single quote << ' >>" << std::endl;
                            error = true;
                            break;
                        case QuoteMode::DOUBLE:
                            std::cerr << "syntax error: Unexpected EOF while looking for matching double quote << \" >>" << std::endl;
                            error = true;
                            break;
                        case QuoteMode::UNQUOTED:
                            break;
                    }
                    break;
                } else {
                    arg = continue_arg(arg, input, delim, &rest, &quoted, &quote, &get_new_line);
                    assert(!arg.empty());
                }
            }

            if (error) continue;

            // handle command execution logic...
        }
    }

    return 0;
}
