#pragma once

#include "noncopyable.h"
#include "timestamp.h"
#include <string>

// ##__VA_ARGS__ 输入任意参数占位符
// LOGINFO("%s %d",arg1,arg2)
#define LOG_INFO(logmsgFormat, ...)                       \
    do                                                    \
    {                                                     \
        Logger &log = Logger::instance();                 \
        log.set_loglevel(INFO);                           \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        log.log(buf);                                     \
    } while (0)

// LOGINFO("%s %d",arg1,arg2)
#define LOG_ERROR(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &log = Logger::instance();                 \
        log.set_loglevel(ERROR);                          \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        log.log(buf);                                     \
    } while (0)

// LOGINFO("%s %d",arg1,arg2)
#define LOG_FATAL(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &log = Logger::instance();                 \
        log.set_loglevel(FATAL);                          \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        log.log(buf);                                     \
        exit(-1);                                         \
    } while (0)

// LOGINFO("%s %d",arg1,arg2)
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                      \
    do                                                    \
    {                                                     \
        Logger &log = Logger::instance();                 \
        log.set_loglevel(DEBUG);                          \
        char buf[1024] = {0};                             \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        log.log(buf);                                     \
    } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

// 定义日志级别 INFO ERROR FATAL DEBUG
enum LogLevel
{
    INFO,  // 普通信息
    ERROR, // 错误信息
    FATAL, // core信息
    DEBUG, // 调试信息
};

// 输出一个日志类
class Logger : noncopyable
{
public:
    //单体模式 获取日志唯一的实例对象
    static Logger &instance()
    {
        static Logger logger; //分配到栈，析构自己释放
        return logger;
    }

    // 设置日志级别
    void set_loglevel(int level) { loglevel_ = level; }

    //写日志
    void log(std::string msg);

private:
    Logger(/* args */) = default;

    int loglevel_ = 0;
};

