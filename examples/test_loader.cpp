#include <dynalo/dynalo.hpp>

#include <cstdint>
#include <sstream>
#include <iostream>
#include <kosongg/GetExeDirectory.h>

// usage: loader "path/to/lib/folder"
int main(int argc, char* argv[])
{
    std::string path = GetExeDirectory(); //  std::filesystem::path(argv[1])
    std::cout << "KIMAX: " <<path << std::endl;

    const std::filesystem::path lib_name = std::filesystem::path(path) / dynalo::to_native_name("demo_ui");
    std::cout << "lib_name:" <<  lib_name.c_str() << std::endl;
    dynalo::library lib(lib_name);

    auto add_integers  = lib.get_function<int32_t(const int32_t, const int32_t)>("add_integers");
    auto print_message = lib.get_function<void(const char*)>("print_message");

    std::ostringstream oss;
    oss << "it works: " << add_integers(1, 2);

    print_message(oss.str().c_str());  // prints: Hello [it works: 3]
    return 0;
}
