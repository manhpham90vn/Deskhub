#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace deskhubcli {

class Table {
public:
    void Row(std::vector<std::string> cells);
    void Print() const;

private:
    std::vector<std::vector<std::string>> rows_{};
};

void PrintLine(std::string_view text);
void PrintError(std::string_view text);

std::string UnixDate(int64_t unixSeconds);

}
