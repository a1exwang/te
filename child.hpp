#pragma once

#include <string>
#include <tuple>

namespace te {
std::tuple<int, int> start_child(std::string command_line, char **argv, char **envp);
}
