#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sstream>
#include <sys/wait.h>
#include <termios.h>
#include <dirent.h>
#include <fcntl.h>
#include <cstring>

using namespace std;

// List of built-in commands
vector<string> builtins = {
    "type",
    "echo",
    "exit",
    "pwd",
    "cd",
};

// Function to get user input character by character
char getChar()
{
    struct termios oldt, newt;
    char ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // Disable line buffering and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

// Function to get all executables from PATH
vector<string> getExecutablesFromPath()
{
    vector<string> executables;
    char *path_env = getenv("PATH");
    if (!path_env) return executables;

    stringstream ss(path_env);
    string dir;

    while (getline(ss, dir, ':'))
    { // Split PATH by ':'
        DIR *dp = opendir(dir.c_str());
        if (dp)
        {
            struct dirent *entry;
            while ((entry = readdir(dp)) != nullptr)
            {
                string filename(entry->d_name);
                string fullPath = dir + "/" + filename;

                if (access(fullPath.c_str(), X_OK) == 0)
                { // Check if executable
                    executables.push_back(filename);
                }
            }
            closedir(dp);
        }
    }

    return executables;
}

// Function to find the longest common prefix
string longestCommonPrefix(const vector<string> &matches)
{
    if (matches.empty())
        return "";
    string prefix = matches[0];
    for (size_t i = 1; i < matches.size(); ++i)
    {
        size_t j = 0;
        while (j < prefix.size() && j < matches[i].size() && prefix[j] == matches[i][j])
        {
            j++;
        }
        prefix = prefix.substr(0, j);
        if (prefix.empty())
            break;
    }
    if(matches.size() == 1) return prefix+" ";
    return prefix;
}

// Autocomplete function
vector<string> autocomplete(const string &input)
{
    vector<string> matches;

    // Check built-in commands
    for (const string &cmd : builtins)
    {
        if (cmd.find(input) == 0)
        {
            matches.push_back(cmd);
        }
    }

    // Check external executables
    vector<string> executables = getExecutablesFromPath();
    for (const string &cmd : executables)
    {
        if (cmd.find(input) == 0)
        {
            matches.push_back(cmd);
        }
    }

    sort(matches.begin(), matches.end());
    return matches;
}

// Helper functions to trim strings
inline void ltrim(string &s)
{
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch)
                               { return !isspace(ch); }));
}

inline void rtrim(string &s)
{
    s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                    { return !isspace(ch); })
                .base(),
            s.end());
}

// Function to split input into arguments while respecting quotes
vector<string> splitInput(const string &input)
{
    vector<string> args;
    string arg;
    bool inQuotes = false;
    bool inSingleQuotes = false;

    for (size_t i = 0; i < input.length(); ++i) {
        char ch = input[i];
        if (inQuotes) {
            if (ch == '"') inQuotes = false;
            else arg += ch;
        } else if (inSingleQuotes) {
            if (ch == '\'' && i + 1 < input.length()) arg += input[++i];
            else if (ch == '\'' && input[i + 1] == '\'') arg += '\'';
            else if (ch == '\'') inSingleQuotes = false;
            else arg += ch;
        } else {
            if (isspace(ch)) {
                if (!arg.empty()) {
                    args.push_back(arg);
                    arg.clear();
                }
            } else if (ch == '"') inQuotes = true;
            else if (ch == '\'' && i + 1 < input.length()) arg += input[++i];
            else if (ch == '>') {
                if (!arg.empty()) {
                    args.push_back(arg);
                    arg.clear();
                }
                args.push_back(">");
            } else arg += ch;
        }
    }
    if (!arg.empty()) args.push_back(arg);
    return args;
}

// Function to check if a command is a built-in
bool isBuiltin(const string &command, const vector<string> &builtins)
{
    return find(builtins.begin(), builtins.end(), command) != builtins.end();
}

// Search for an executable in the PATH
string findExecutable(const string &command)
{
    char *path_env = getenv("PATH");
    if (path_env == nullptr)
        return "";

    string path(path_env);
    size_t pos = 0;

    while ((pos = path.find(':')) != string::npos)
    {
        string dir = path.substr(0, pos);
        path.erase(0, pos + 1);

        string file_path = dir + "/" + command;
        if (access(file_path.c_str(), R_OK | X_OK) == 0)
        {
            return file_path; // Return the first match found
        }
    }

    // Check the remaining part of PATH
    if (!path.empty())
    {
        string file_path = path + "/" + command;
        if (access(file_path.c_str(), R_OK | X_OK) == 0)
        {
            return file_path; // Return the first match found
        }
    }

    return ""; // No executable found
}

// Execute external commands
void executeExternal(vector<string> &args) {
    int outFd = -1;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == ">" || args[i] == "1>") {
            if (i + 1 < args.size()) {
                outFd = open(args[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (outFd == -1) {
                    perror("Failed to open file for writing");
                    return;
                }
                args.erase(args.begin() + i, args.begin() + i + 2);
                break;
            } else {
                cerr << "Syntax error: expected file after >\n";
                return;
            }
        }
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("Fork failed");
        return;
    }
    if (pid == 0) { // Child process
        if (outFd != -1) {
            dup2(outFd, STDOUT_FILENO);
            close(outFd);
        }
        vector<char *> c_args;
        for (string &arg : args) c_args.push_back(&arg[0]);
        c_args.push_back(nullptr);
        execvp(c_args[0], c_args.data());
        perror("Execution failed");
        exit(EXIT_FAILURE);
    } else {
        waitpid(pid, nullptr, 0);
        if (outFd != -1) close(outFd);
    }
}


// Finding Present working directory
void pwd()
{
    char pwd[PATH_MAX];
    if (getcwd(pwd, sizeof(pwd)) != NULL)
    {
        cout << pwd << endl;
    }
    else
    {
        perror("getcwd() error");
    }
}

// Echo command
void echo(const vector<string> &args)
{
    for (size_t i = 1; i < args.size(); ++i)
    {
        if (i > 1)
            cout << " ";
        cout << args[i];
    }
    cout << endl;
}

int main()
{

    // Flush after every std::cout / std::cerr
    cout << unitbuf;
    cerr << unitbuf;

    while (true)
    {
        cout << "$ ";

        string input;
        char ch;
        static int tabPressCount = 0;
        while (true)
        {
            ch = getChar();

            if (ch == '\n')
            {
                cout << endl;
                break;
            }
            else if (ch == '\t')
            {
                vector<string> matches = autocomplete(input);
    matches.erase(unique(matches.begin(), matches.end()), matches.end());

    if (matches.empty())
    {
        cout << "\a"; // Ring bell if no matches
        tabPressCount = 0;
    }
    else
    {
        string commonPrefix = longestCommonPrefix(matches);
        
        if (commonPrefix.length() > input.length())
        {
            cout << commonPrefix.substr(input.length());
            input = commonPrefix;
            tabPressCount = 0; // Reset counter after successful completion
        }
        else
        {
            if (tabPressCount == 1)
            {
                cout << "\a\n"; 
                for (const string &cmd : matches)
                {
                    cout << cmd << "  ";
                }
                cout << endl << "$ " << input;
                tabPressCount = 0; 
            }
            else
            {
                cout << "\a";
                tabPressCount++;
            }
        }
    }
            }
            else if (ch == 127)
            {
                if (!input.empty())
                {
                    input.pop_back();
                    cout << "\b \b";
                }
                tabPressCount = 0;
            }
            else
            {
                input += ch;
                cout << ch;
                tabPressCount = 0;
            }
        }

        // Exiting the shell
        if (input == "exit 0")
        {
            exit(0);
        }

        // Parse the input into arguments
        vector<string> args = splitInput(input);
        if (args.empty())
            continue;

        string command = args[0];

        // Handle the `echo` command
        if (command == "echo")
        {
            echo(args);
        }

        // Handle the `cat` command
        else if (command == "cat")
        {
            executeExternal(args);
        }

        // Handle the `pwd` command
        else if (command == "pwd")
        {
            if (args.size() == 1)
                pwd();
            else
            {
                cerr << "pwd: No parameters required." << endl;
            }
        }

        // Handle the `cd` command
        else if (command == "cd")
        {
            if (args.size() == 1 || args[1] == "~")
            {
                const char *HOMEPATH = getenv("HOME");
                if (HOMEPATH && chdir(HOMEPATH) == 0)
                {
                    // Successfully changed to home directory
                }
                else
                {
                    cerr << command << ": Unable to access home directory" << endl;
                }
            }
            else if (chdir(args[1].c_str()) != 0)
            {
                cerr << command << ": " << args[1] << ": No such file or directory" << endl;
            }
        }

        // Handle the `type` command
        else if (command == "type")
        {
            if (args.size() == 1)
            {
                cerr << "type: missing argument\n";
                continue;
            }

            if (isBuiltin(args[1], builtins))
            {
                cout << args[1] << " is a shell builtin\n";
            }
            else if (args[1].find('/') != string::npos && access(args[1].c_str(), R_OK | X_OK) == 0)
            {
                cout << args[1] << " is " << args[1] << "\n";
            }
            else
            {
                string execPath = findExecutable(args[1]);
                if (!execPath.empty())
                {
                    cout << args[1] << " is " << execPath << "\n";
                }
                else
                {
                    cerr << args[1] << ": not found\n";
                }
            }
        }

        // Handle unknown or external commands
        else
        {
            string execPath = findExecutable(command);
            if (!execPath.empty())
            {
                executeExternal(args);
            }
            else
            {
                cerr << command << ": command not found\n";
            }
        }
    }

    return 0;
}
