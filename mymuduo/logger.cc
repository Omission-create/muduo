#include "logger.h"
#include <iostream>

//[级别信息] time : msg
void Logger::log(std::string msg)
{
    switch (loglevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    default:
        break;
    }

    //打印时间和msg
    std::cout << Timestamp::now().toString()
              << ":" << msg << std::endl;
}