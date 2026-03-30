#include "spacesim/render/WindowCommandBridge.hpp"

#include <mutex>
#include <queue>

namespace spacesim::render::window_commands {

namespace {

std::mutex gQueueMutex;
std::queue<std::string> gQueue;

} // namespace

void enqueue(const std::string& command) {
    std::lock_guard<std::mutex> lock(gQueueMutex);
    gQueue.push(command);
}

bool tryPop(std::string& outCommand) {
    std::lock_guard<std::mutex> lock(gQueueMutex);
    if (gQueue.empty()) {
        return false;
    }

    outCommand = gQueue.front();
    gQueue.pop();
    return true;
}

} // namespace spacesim::render::window_commands
