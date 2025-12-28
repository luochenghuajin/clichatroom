// console.h

#pragma once
#include <string>

namespace Console {
    // 函数声明 (Declaration) - 只有函数签名，以分号结尾
    std::string ReadLine();
    void Print(const std::string& text);
}