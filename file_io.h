#ifndef FILE_IO_H_
#define FILE_IO_H_

#include <string>
#include <vector>

namespace File {

void OpenAppend(const std::string& filename);
void AppendLine(const std::string& filename, const std::string& line);

} // namespace File

#endif // FILE_IO_H_