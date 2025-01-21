#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <locale>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

// Helper functions to trim strings
inline void ltrim(string &s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !isspace(ch);
    }));
}

inline void rtrim(string &s) {
    s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !isspace(ch);
    }).base(), s.end());
}

// Split a string into tokens by spaces
vector<string> splitInput(const string &input) {
    vector<string> tokens;
    size_t start = 0, end;
    while ((end = input.find(' ', start)) != string::npos) {
        if (start != end) {
            tokens.push_back(input.substr(start, end - start));
        }
        start = end + 1;
    }
    if (start < input.size()) {
        tokens.push_back(input.substr(start));
    }
    return tokens;
}

// Function to check if a command is a built-in
bool isBuiltin(const string &command, const vector<string> &builtins) {
    return find(builtins.begin(), builtins.end(), command) != builtins.end();
}

// Execute external commands using fork and execvp
void executeExternal(const vector<string> &args) {
    pid_t pid = fork();

    if (pid == -1) {
        cerr << "Error: failed to fork process\n";
        return;
    }

    if (pid == 0) { // Child process
        // Convert vector<string> to char* array for execvp
        vector<char *> c_args;
        for (const string &arg : args) {
            c_args.push_back(const_cast<char *>(arg.c_str()));
        }
        c_args.push_back(nullptr); // Null-terminate the array

        if (execvp(c_args[0], c_args.data()) == -1) {
            perror("Error");
            exit(EXIT_FAILURE);
        }
    } else { // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for child to finish
    }
}

int main() {
    // List of built-in commands
    vector<string> builtins = {"type", "echo", "exit"};

    // Flush after every std::cout / std::cerr
    cout << unitbuf;
    cerr << unitbuf;

    while (true) {
        cout << "$ ";

        string input;
        getline(cin, input);

        // Exiting the shell
        if (input == "exit 0") {
            exit(0);
        }

        // Parse input into tokens
        vector<string> args = splitInput(input);
        if (args.empty()) {
            continue;
        }

        string command = args[0];

        // Handle built-in `echo`
        if (command == "echo") {
            for (size_t i = 1; i < args.size(); ++i) {
                cout << args[i] << (i < args.size() - 1 ? " " : "\n");
            }
        }
        // Handle built-in `type`
        else if (command == "type") {
            if (args.size() < 2) {
                cerr << "type: missing argument\n";
                continue;
            }
            string target = args[1];
            if (isBuiltin(target, builtins)) {
                cout << target << " is a shell builtin\n";
            } else if (target.find('/') != string::npos && access(target.c_str(), R_OK | X_OK) == 0) {
                cout << target << " is " << target << "\n";
            } else {
                cerr << target << ": not found\n";
            }
        }
        // Handle external commands
        else {
            executeExternal(args);
        }
    }
}
