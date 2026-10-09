#include "tpc/tpc.hpp"

#include <cmath>
#include <cstdlib>
#include <string>

namespace {

bool is_near(double actual, double expected, double tolerance) {
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    const auto frame = tpc::system::TPC::create_test_frame();

    if (frame.size() != 36)
        return EXIT_FAILURE;

    for (const char base : {'E', 'W'}) {
        for (int sensor = 1; sensor <= 6; ++sensor) {
            const auto prefix = std::string{base} + std::to_string(sensor);

            if (!frame.contains(prefix + 'R') || !frame.contains(prefix + 'F') || !frame.contains(prefix + 'Z'))
                return EXIT_FAILURE;

            if (!is_near(frame.at(prefix + 'F'), 0.0, 0.03))
                return EXIT_FAILURE;

            if (!is_near(frame.at(prefix + 'Z'), 5000.0, 0.03))
                return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
