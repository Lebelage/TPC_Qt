#pragma once

#include <array>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <expected>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tpc_system/models/data.hpp"

namespace tpc_qt::models {

enum class SensorNameKey : char { W = 'W', E = 'E' };

enum class SensorNameComponent : char { R = 0, F = 1, Z = 2 };

struct SensorName {
    SensorNameKey id{SensorNameKey::W};
    std::uint16_t number{};

    [[nodiscard]]
    std::string to_string() const {
        std::string result;
        result.reserve(3);

        result += static_cast<char>(id);
        result += std::to_string(number);

        return result;
    }

    [[nodiscard]]
    constexpr std::size_t component_index(SensorNameComponent component) const noexcept {
        switch (component) {
            case SensorNameComponent::R:
                return 0;
            case SensorNameComponent::F:
                return 1;
            case SensorNameComponent::Z:
                return 2;
        }

        return 0;
    }

    [[nodiscard]]
    static std::expected<SensorName, std::string> parse_name(std::string_view text) {
        const auto error = [&] {
            return std::unexpected{"Invalid sensor name: " + std::string{text}};
        };

        if (text.size() < 3) {
            return error();
        }

        SensorName result;

        switch (text.front()) {
            case 'W':
                result.id = SensorNameKey::W;
                break;

            case 'E':
                result.id = SensorNameKey::E;
                break;

            default:
                return error();
        }

        const std::string_view number_text = text.substr(1, text.size() - 2);

        const char* begin = number_text.data();
        const char* end = begin + number_text.size();

        const auto [ptr, ec] = std::from_chars(begin, end, result.number);

        if (ec != std::errc{} || ptr != end) {
            return error();
        }

        return result;
    }

    static std::expected<SensorNameComponent, std::string> take_index_from_component(std::string_view text) {
        const auto error = [&] {
            return std::unexpected{"Invalid sensor name: " + std::string{text}};
        };

        if (text.size() < 3) {
            return error();
        }

        switch (text.back()) {
            case 'R':
                return SensorNameComponent::R;
            case 'F':
                return SensorNameComponent::F;
            case 'Z':
                return SensorNameComponent::Z;
            default:
                return error();
        }
    }

    template <std::ranges::input_range Range>
        requires std::convertible_to<std::ranges::range_reference_t<Range>, std::string_view>
    [[nodiscard]]
    static std::expected<std::vector<SensorName>, std::string> parse_names_range(Range&& names) {
        std::vector<SensorName> result;

        if constexpr (std::ranges::sized_range<Range>) {
            result.reserve(std::ranges::size(names));
        }

        for (const auto& text : names) {
            auto parsed = parse_name(std::string_view{text});

            if (!parsed) {
                return std::unexpected{parsed.error()};
            }

            result.push_back(*parsed);
        }

        std::ranges::sort(result, [](const SensorName& lhs, const SensorName& rhs) {
            if (lhs.id != rhs.id) {
                return static_cast<char>(lhs.id) < static_cast<char>(rhs.id);
            }

            return lhs.number < rhs.number;
        });

        const auto duplicates = std::ranges::unique(result, [](const SensorName& lhs, const SensorName& rhs) {
            return lhs.id == rhs.id && lhs.number == rhs.number;
        });

        result.erase(duplicates.begin(), duplicates.end());

        return result;
    }
};

struct Sensor {
    SensorName name{};
    std::array<double, 3> position{};
    std::array<double, 3> values{};

    void set_value(double value, SensorNameComponent component) noexcept {
        values[name.component_index(component)] = value;
    }
};

class TpcDataModel {
public:
    using DiscoveryResult = tpc::system::models::DiscoveryResult;

    using ReceivedFrame = std::unordered_map<std::string, double>;

    [[nodiscard]]
    const DiscoveryResult& discovery_result() const noexcept {
        return discovery_result_;
    }

    void set_discovery_result(DiscoveryResult result) {
        discovery_result_ = std::move(result);
    }

    [[nodiscard]]
    std::span<const Sensor> sensors() const noexcept {
        return sensors_;
    }

    void set_sensors(std::vector<Sensor> sensors) {
        sensors_ = std::move(sensors);
        apply_received_frame();
    }

    void set_received_frame(ReceivedFrame frame) {
        received_frame_ = std::move(frame);
        apply_received_frame();
    }

    void clear() noexcept {
        discovery_result_ = {};
        received_frame_.clear();
        sensors_.clear();
    }

private:
    void apply_received_frame() {
        for (const auto& [text_name, value] : received_frame_) {
            auto name = SensorName::parse_name(text_name);
            auto component = SensorName::take_index_from_component(text_name);

            if (!name || !component) {
                continue;
            }

            const auto iterator = std::ranges::find_if(sensors_, [&name](const Sensor& sensor) {
                return sensor.name.id == name->id && sensor.name.number == name->number;
            });

            if (iterator == sensors_.end()) {
                continue;
            }

            iterator->set_value(value, *component);
        }
    }

    DiscoveryResult discovery_result_{};
    ReceivedFrame received_frame_{};
    std::vector<Sensor> sensors_{};
};

}  // namespace tpc_qt::models