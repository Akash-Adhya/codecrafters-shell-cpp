#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <locale>
#include <cstdlib>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

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

// Execute external commands
void executeExternal(const vector<string> &args)
{
    pid_t pid = fork();

    if (pid == -1)
    {
        cerr << "Error: failed to fork process\n";
        return;
    }

    if (pid == 0) // Child process
    {
        // Convert vector<string> to char* array for execvp
        vector<char *> c_args;
        for (const string &arg : args)
        {
            c_args.push_back(const_cast<char *>(arg.c_str()));
        }
        c_args.push_back(nullptr); // Null-terminate the array

        if (execvp(c_args[0], c_args.data()) == -1)
        {
            perror("Error");
            exit(EXIT_FAILURE);
        }
    }
    else // Parent process
    {
        int status;
        waitpid(pid, &status, 0); // Wait for child to finish
    }
}

// Finding Present working directory 
void pwd(){
    char pwd[PATH_MAX];
    if(getcwd(pwd, sizeof(pwd)) != NULL){
        cout<<pwd<<endl;
    }
    else{
        perror("getcwd() error");
    }
}

int main()
{
    // List of built-in commands
    vector<string> builtins = {"type", "echo", "exit"};

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

        // Split input into command and arguments
        vector<string> args;
        args.push_back(command);
        size_t pos = 0;
        string temp = parameters;
        while ((pos = temp.find(' ')) != string::npos)
        {
            args.push_back(temp.substr(0, pos));
            temp.erase(0, pos + 1);
        }
        if (!temp.empty())
        {
            args.push_back(temp);
        }

        // Handle the `echo` command
        if (command == "echo")
        {
            cout << parameters << endl;
        }

        //handle the `pwd` command
        if (command == "pwd"){
            if(parameters.empty())
                pwd();
            else{
                cerr<<"pwd : No parameters required."<<endl;
            }
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
