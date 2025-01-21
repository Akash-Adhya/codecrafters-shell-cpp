#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <locale>
#include <cstdlib>
#include <unistd.h>

using namespace std;

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

// Partition the command from the input
string partitionCommand(const string &input)
{
    string command = "";
    int i = 0;

    while (i < input.length() && input[i] != ' ')
    {
        command += input[i];
        i++;
    }

    return command;
}

// Partition parameters from the input
string partitionParameters(string input, int index)
{
    return input.substr(index);
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






int main()
{
    // List of built-in commands
    vector<string> builtins = {"type", "echo", "exit", "cat"};

    // Flush after every std::cout / std::cerr
    cout << unitbuf;
    cerr << unitbuf;

    while (true)
    {
        cout << "$ ";

        string input;
        getline(cin, input);

        // Exiting the shell
        if (input == "exit 0")
        {
            exit(0);
        }

        // Capture command and parameters
        string command = partitionCommand(input);
        string parameters = partitionParameters(input, command.length());

        ltrim(command);
        rtrim(command);
        ltrim(parameters);
        rtrim(parameters);

        // Handle the `echo` command
        if (command == "echo")
        {
            cout << parameters << endl;
        }

        // Handle the `type` command
        else if (command == "type")
        {
            if (parameters.empty())
            {
                cerr << "type: missing argument\n";
                continue;
            }

            if (isBuiltin(parameters, builtins))
            {
                cout << parameters << " is a shell builtin\n";
            }
            else if (parameters.find('/') != string::npos && access(parameters.c_str(), R_OK | X_OK) == 0)
            {
                cout << parameters << " is " << parameters << "\n";
            }
            else
            {
                string execPath = findExecutable(parameters);
                if (!execPath.empty())
                {
                    cout << parameters << " is " << execPath << "\n";
                }
                else
                {
                    cerr << parameters << ": not found\n";
                }
            }
        }

        // Handle unknown commands
        else
        {
            cerr << command << ": command not found\n";
        }
    }
}
