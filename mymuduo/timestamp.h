#pragma once
#pragma once
#include <iostream>
#include <string>

class Timestamp
{
public:
    Timestamp();

    // explicit 防止隐式类型转换
    explicit Timestamp(int64_t microSecondsSinceEpoch);

    ///
    /// Get time of now.
    ///
    static Timestamp now();

    std::string toString() const;

private:
    int64_t microSecondsSinceEpoch_ = 0;
};
