# Concept #

A C++ library providing allocator types and helper functions using them.

# Install #
## Requirements ##

Binaries:

- A C++20 compiler (ex: g++-13)
- CMake 3.26 or later

Testing Libraries (optional):

- [Google Test](https://github.com/google/googletest) 1.13 or later (optional)

## Clone

```
git clone https://github.com/arapelle/arba-aloc --recurse-submodules
```

## Quick Install ##
There is a cmake script at the root of the project which builds the library in *Release* mode and install it (default options are used).
```
cd /path/to/arba-aloc
cmake -P cmake/scripts/quick_install.cmake
```
Use the following to quickly install a different mode.
```
cmake -P cmake/scripts/quick_install.cmake -- TESTS BUILD Debug DIR /tmp/local
```

## Uninstall ##
There is a uninstall cmake script created during installation. You can use it to uninstall properly this library.
```
cd /path/to/installed-arba-aloc/
cmake -P uninstall.cmake
```

# How to use
## Example - ???
```c++
#include <arba/aloc/version.hpp>
#include <iostream>

int main()
{
    std::cout << "arba-aloc-" << ARBA_ALOC_VERSION << std::endl;
    return EXIT_SUCCESS;
}

```

## Example - Using *arba-aloc* in a CMake project
See *basic_cmake_project* in example, and more specifically the *CMakeLists.txt* to see how to use *arba-aloc* in your CMake projects.

# License

[MIT License](./LICENSE.md) © arba-aloc
