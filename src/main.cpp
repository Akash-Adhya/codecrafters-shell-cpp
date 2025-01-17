#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <locale>

using namespace std;

inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                    { return !std::isspace(ch); }));
}

inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                         { return !std::isspace(ch); })
                .base(),
            s.end());
}

string partitionCommand(const string &input)
{
    int n = input.length();
    string command = "";
    int i = 0;
    char c;
    while (i < n && c != ' ')
    {
        c = input[i];
        command += c;
        i++;
    }

    return command;
}

string partitionParameters(string input, int index)
{
    int i = index;
    char c;
    string parameters;

    while (i < input.length())
    {
        c = input[i];
        parameters += c;
        i++;
    }

    return parameters;
}

int main()
{
    // Flush after every std::cout / std:cerr
    cout << unitbuf;
    cerr << unitbuf;

    // Uncomment this block to pass the first stage

    while (true)
    {
        cout << "$ ";

        string input;
        string command = "";
        string parameters = "";
        string err_msg = ": command not found\n";

        getline(cin, input);

        // exiting from loop
        if (input == "exit 0")
        {
            exit(0);
        }

        // capturing command
        command = partitionCommand(input);

        // capturing parameters
        parameters = partitionParameters(input, command.length());

        ltrim(command);
        rtrim(command);
        ltrim(parameters);
        rtrim(parameters);

        // echo command
        if (command == "echo")
            cout << parameters << endl;

        else
            cout << command << err_msg << endl;
    }
}
