#pragma once

#include "result.hpp"
#include <string>
#include <atomic>
#include <random>
#include <sstream>
#include <iomanip>

namespace ymery {

// Base class for all ymery objects with lifecycle management
class Object {
public:
    Object() : uid_(generate_uid()) {}
    virtual ~Object() = default;

    // Unique identifier
    const std::string& uid() const { return uid_; }

    // Lifecycle - override in subclasses
    virtual Result<void> init() { return Ok(); }
    virtual Result<void> dispose() { return Ok(); }

protected:
    std::string uid_;

private:
    static std::string generate_uid() {
        static std::atomic<uint64_t> counter{0};
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 35);

        std::ostringstream oss;
        for (int i = 0; i < 10; ++i) {
            int v = dis(gen);
            oss << static_cast<char>(v < 10 ? '0' + v : 'a' + v - 10);
        }
        return oss.str();
    }
};

} // namespace ymery
