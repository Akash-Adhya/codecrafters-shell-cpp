#include <iostream>
#include <vector>
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
  string command = "";
  int i = 0;

  while (i < input.length() && input[i] != ' ')
  {
    command += input[i];
    i++;
  }

  return command;
}


string partitionParameters(string input, int index)
{
  // int i = index;
  // char c;
  // string parameters;

  // while (i < input.length())
  // {
  //   c = input[i];
  //   parameters += c;
  //   i++;
  // }

  // return parameters;
  return input.substr(index);
}

int main()
{

  vector<string> commands = {"type", "echo", "exit"};
  // Flush after every std::cout / std:cerr
  cout << unitbuf;
  cerr << unitbuf;

  // Uncomment this block to pass the first stage

  while (true)
  {
    cout << "$ ";

    string input;
    string err_msg = ": command not found\n";

    getline(cin, input);

    // exiting from loop
    if (input == "exit 0")
    {
      exit(0);
    }

    // capturing command
    string command = "";
    command = partitionCommand(input);

    // capturing parameters
    string parameters = "";
    parameters = partitionParameters(input, command.length());

    ltrim(command);
    rtrim(command);
    ltrim(parameters);
    rtrim(parameters);

    // echo command
    if (command == "echo")
      cout << parameters << endl;

    // type command
    else if(command == "type"){
      if(find(commands.begin(), commands.end(), parameters) != commands.end()){
        cout<<parameters<<" is a shell builtin\n";
      }
      else{
        cout<<parameters<<": not found\n";
      }
    }

    else
      cout << command << err_msg;
  }
}
