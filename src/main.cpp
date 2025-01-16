#include <iostream>
#include <string>

int main()
{
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // Uncomment this block to pass the first stage

  while (true)
  {
    std::cout << "$ ";

    std::string input;
    std::getline(std::cin, input);

    // exiting from loop
    if (input == "exit 0")
    {
      exit(0);
    }


    std::string err_msg = ": command not found\n";
    std::cout << input << err_msg;
  }
}
