#pragma once

/**
 * @brief noncopybale被继承后，派生类对象可以正常构造和析构
 *      但是派生类对象无法进行拷贝构造和赋值操作
 */
class noncopyable
{
private:
    /* data */
public:
    noncopyable(/* args */) = default;
    ~noncopyable() = default;

    noncopyable(const noncopyable &) = delete;
    void operator=(const noncopyable &) = delete;
};
