#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

using namespace std;

// Helper function to trim strings
inline void trim(string &s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch) { return !isspace(ch); }));
    s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !isspace(ch); }).base(), s.end());
}

// Split input into arguments while respecting quotes
vector<string> parseInput(const string &input) {
    vector<string> args;
    string arg;
    bool inDoubleQuotes = false, inSingleQuotes = false;

    for (size_t i = 0; i < input.size(); ++i) {
        char ch = input[i];
        if (inDoubleQuotes) {
            if (ch == '"') inDoubleQuotes = false;
            else if (ch == '\\' && i + 1 < input.size() && (input[i + 1] == '"' || input[i + 1] == '\\')) arg += input[++i];
            else arg += ch;
        } else if (inSingleQuotes) {
            if (ch == '\'') inSingleQuotes = false;
            else arg += ch;
        } else {
            if (isspace(ch)) {
                if (!arg.empty()) args.push_back(arg), arg.clear();
            } else if (ch == '"') inDoubleQuotes = true;
            else if (ch == '\'') inSingleQuotes = true;
            else arg += ch;
        }
    }
    if (!arg.empty()) args.push_back(arg);
    return args;
}

// Check if a command is a built-in
inline bool isBuiltin(const string &command, const vector<string> &builtins) {
    return find(builtins.begin(), builtins.end(), command) != builtins.end();
}

// Execute external commands
void executeExternal(const vector<string> &args) {
    if (fork() == 0) {
        vector<char *> c_args;
        for (const auto &arg : args) c_args.push_back(const_cast<char *>(arg.c_str()));
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        perror("Error");
        exit(EXIT_FAILURE);
    } else {
        int status;
        wait(&status);
    }
}

// Built-in commands
void pwd() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) cout << cwd << endl;
    else perror("getcwd() error");
}

void echo(const vector<string> &args) {
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) cout << " ";
        cout << args[i];
    }
    cout << endl;
}

void cd(const vector<string> &args) {
    const char *path = args.size() > 1 ? (args[1] == "~" ? getenv("HOME") : args[1].c_str()) : getenv("HOME");
    if (chdir(path) != 0) cerr << "cd: " << args[1] << ": No such file or directory" << endl;
}

void type(const vector<string> &args, const vector<string> &builtins) {
    if (args.size() < 2) {
        cerr << "type: missing argument\n";
        return;
    }
    const string &command = args[1];
    if (isBuiltin(command, builtins)) cout << command << " is a shell builtin\n";
    else {
        char *path_env = getenv("PATH");
        stringstream path_stream(path_env ? path_env : "");
        string dir;
        while (getline(path_stream, dir, ':')) {
            string full_path = dir + "/" + command;
            if (access(full_path.c_str(), X_OK) == 0) {
                cout << command << " is " << full_path << "\n";
                return;
            }
        }
        cerr << command << ": not found\n";
    }
}

int main() {
    vector<string> builtins = {"type", "echo", "exit", "pwd", "cd", "cat"};
    cout << unitbuf;

    while (true) {
        cout << "$ ";
        string input;
        getline(cin, input);

        if (input.empty()) continue;
        auto args = parseInput(input);
        const string &command = args[0];

        if (command == "exit") break;
        else if (command == "pwd") pwd();
        else if (command == "echo") echo(args);
        else if (command == "cd") cd(args);
        else if (command == "type") type(args, builtins);
        else if (command == "cat" || !isBuiltin(command, builtins)) executeExternal(args);
        else cerr << command << ": command not found\n";
    }

    return 0;
}
