#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tpc_slint::models {

enum class SensorNameKey : char { W = 'W', E = 'E' };
enum class SensorNameComponent : std::size_t { R, F, Z };
enum class SensorCoordinate : std::size_t { X, Y, Z };

/** Parsed sensor identity without its measured component suffix. */
struct SensorName {
    SensorNameKey id{SensorNameKey::W};
    std::uint16_t number{};

    [[nodiscard]] std::string toString() const {
        return std::string{static_cast<char>(id)} + std::to_string(number);
    }

    [[nodiscard]] static std::expected<SensorName, std::string> parse(std::string_view text) {
        const auto invalidName = [text] {
            return std::unexpected{"Invalid sensor name: " + std::string{text}};
        };

        // Accept both a physical sensor name (W12) and a channel name (W12R).
        if (text.size() < 2) {
            return invalidName();
        }

        SensorName result;
        switch (text.front()) {
            case 'W': result.id = SensorNameKey::W; break;
            case 'E': result.id = SensorNameKey::E; break;
            default: return invalidName();
        }

        const bool has_component = text.back() == 'R' || text.back() == 'F' || text.back() == 'Z';
        const std::size_t number_length = text.size() - 1 - static_cast<std::size_t>(has_component);
        const std::string_view number_text = text.substr(1, number_length);
        const auto [end, error] = std::from_chars(
            number_text.data(), number_text.data() + number_text.size(), result.number
        );

        if (error != std::errc{} || end != number_text.data() + number_text.size()) {
            return invalidName();
        }

        return result;
    }

    [[nodiscard]] static std::expected<SensorNameComponent, std::string> parseComponent(std::string_view text) {
        if (text.size() < 3) {
            return std::unexpected{"Invalid sensor name: " + std::string{text}};
        }

        switch (text.back()) {
            case 'R': return SensorNameComponent::R;
            case 'F': return SensorNameComponent::F;
            case 'Z': return SensorNameComponent::Z;
            default: return std::unexpected{"Invalid sensor name: " + std::string{text}};
        }
    }

    template <std::ranges::input_range Range>
        requires std::convertible_to<std::ranges::range_reference_t<Range>, std::string_view>
    [[nodiscard]] static std::expected<std::vector<SensorName>, std::string> parseNames(Range&& names) {
        std::vector<SensorName> result;
        if constexpr (std::ranges::sized_range<Range>) {
            result.reserve(std::ranges::size(names));
        }

        for (const auto& text : names) {
            auto parsed = parse(std::string_view{text});
            if (!parsed) {
                return std::unexpected{parsed.error()};
            }
            result.push_back(*parsed);
        }

        std::ranges::sort(result, {}, [](const SensorName& name) {
            return std::pair{static_cast<char>(name.id), name.number};
        });
        const auto duplicates = std::ranges::unique(result, {}, [](const SensorName& name) {
            return std::pair{static_cast<char>(name.id), name.number};
        });
        result.erase(duplicates.begin(), duplicates.end());
        return result;
    }
};

struct Sensor {
    SensorName name{};
    std::array<double, 3> position{};
    std::array<double, 3> values{};

    void setValue(double value, SensorNameComponent component) noexcept {
        values[static_cast<std::size_t>(component)] = value;
    }

    [[nodiscard]] double value(SensorNameComponent component) const noexcept {
        return values[static_cast<std::size_t>(component)];
    }

    [[nodiscard]] double coordinate(SensorCoordinate coordinate) const noexcept {
        return position[static_cast<std::size_t>(coordinate)];
    }
};

/** Calculation-oriented snapshot of configured sensors and latest readings. */
class TpcDataModel final {
public:
    using ReceivedFrame = std::unordered_map<std::string, double>;

    [[nodiscard]] std::span<const Sensor> sensors() const noexcept { return sensors_; }
    [[nodiscard]] std::array<std::size_t, 3> grid() const noexcept { return grid_; }
    [[nodiscard]] double length() const noexcept { return length_; }
    [[nodiscard]] double radius() const noexcept { return radius_; }

    void setSensors(std::vector<Sensor> sensors) {
        sensors_ = std::move(sensors);
        applyReceivedFrame();
    }

    void setReceivedFrame(ReceivedFrame frame) {
        received_frame_ = std::move(frame);
        applyReceivedFrame();
    }

    void setGrid(std::array<std::size_t, 3> grid) noexcept { grid_ = grid; }

    void setGeometry(double length, double radius) noexcept {
        length_ = length;
        radius_ = radius;
    }

private:
    void applyReceivedFrame() {
        for (const auto& [text_name, value] : received_frame_) {
            const auto name = SensorName::parse(text_name);
            const auto component = SensorName::parseComponent(text_name);
            if (!name || !component) {
                continue;
            }

            const auto sensor = std::ranges::find_if(sensors_, [&name](const Sensor& candidate) {
                return candidate.name.id == name->id && candidate.name.number == name->number;
            });
            if (sensor != sensors_.end()) {
                sensor->setValue(value, *component);
            }
        }
    }

    ReceivedFrame received_frame_;
    std::vector<Sensor> sensors_;
    std::array<std::size_t, 3> grid_{};
    double length_{};
    double radius_{};
};

}  // namespace tpc_slint::models
