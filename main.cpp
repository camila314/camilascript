#include <fstream>
#include <iostream>

import TokenStream;
import Parser;
import State;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Need a file\n";
    } 

    std::basic_ifstream<char16_t> file(argv[1], std::ios::binary);
    std::u16string content((std::istreambuf_iterator<char16_t>(file)), std::istreambuf_iterator<char16_t>());

    State state{TokenStream{content}};

    try {
        while (!state.stream().eof()) {
            parse(state);
        }
    } catch (const Error& e) {
        std::cerr << "Error at " << e.position.second << ":" << e.position.first << ": " << e.what() << std::endl;
    }

    return 0;
}
