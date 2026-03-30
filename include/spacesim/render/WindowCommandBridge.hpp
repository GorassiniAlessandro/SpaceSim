#pragma once

#include <string>

namespace spacesim::render::window_commands {

void enqueue(const std::string& command);
bool tryPop(std::string& outCommand);

} // namespace spacesim::render::window_commands
