#include "file_io.h"
#include <fstream>    // For C++ file I/O
#include <iostream>   // [新增] 为了使用 std::cerr 打印错误信息

namespace File {

void OpenAppend(const std::string& filename) {
    std::ofstream logfile(filename, std::ios::app);
    if (!logfile.is_open()) {
        // [修正] 补全错误信息打印
        // std::cerr 是专门用于输出错误信息的标准流
        std::cerr << "Error: Could not open log file for writing: " << filename << std::endl;
    }
    // The file is automatically closed when `logfile` goes out of scope.
}

void AppendLine(const std::string& filename, const std::string& line) {
    std::ofstream logfile(filename, std::ios::app); // Open in append mode
    if (logfile.is_open()) {
        logfile << line << std::endl;
    } else {
        // [修正] 同样为写入失败增加错误处理
        std::cerr << "Error: Could not write to log file: " << filename << std::endl;
    }
}

} // namespace File