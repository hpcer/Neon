#include <iostream>
#include <string>
class Log
{
public:
    template<typename T>
    void operator << (T str)
    {
        std::cout << str << std::endl;
    }
};