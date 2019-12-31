// A change.

#include "hello.h"
#include <iostream>

void
hello()
{
#ifdef NDEBUG
    std::cout << "Hello World Release!" << std::endl;
#else
    std::cout << "Hello World Debug!" << std::endl;
#endif
}
