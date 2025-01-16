#include <iostream>
#include<string>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // Uncomment this block to pass the first stage
  std::cout << "$ ";

  std::string input;
  std::getline(std::cin, input);
  std::string err_msg = ":  command not found\n";

  std::cout<<input<<err_msg;
}
