#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace tpc_slint::models {

/** Immutable Cartesian reference grid. No extrapolation beyond measured bounds. */
class ReferenceFieldMap final {
public:
    static std::expected<std::shared_ptr<const ReferenceFieldMap>, std::string> load(const std::filesystem::path& path) {
        try {
            if (!path.is_absolute()) return std::unexpected("Reference map path must be absolute");
            if (std::filesystem::file_size(path) > 512ULL * 1024 * 1024)
                return std::unexpected("Reference map exceeds the file size budget");
            std::ifstream input{path};
            if (!input) return std::unexpected("Cannot open reference field map");
            nlohmann::json json;
            input >> json;
            const auto unit = json.at("coordinate_unit").get<std::string>();
            if ((unit != "mm" && unit != "cm") || json.at("field_unit") != "G"
                || json.at("components") != "Cartesian")
                return std::unexpected("Reference map must declare mm (or legacy cm), G and Cartesian components");
            auto result = std::make_shared<ReferenceFieldMap>();
            result->axes_ = {json.at("x").get<std::vector<double>>(), json.at("y").get<std::vector<double>>(),
                json.at("z").get<std::vector<double>>()};
            std::size_t count = 1;
            for (auto& axis : result->axes_) {
                if (unit == "cm") for (auto& value : axis) value *= 10.0;
                if (axis.size() < 2 || axis.size() > 1024 || count > 4'000'000 / axis.size())
                    return std::unexpected("Invalid reference grid dimensions");
                for (std::size_t i = 0; i < axis.size(); ++i)
                    if (!std::isfinite(axis[i]) || (i && axis[i] <= axis[i - 1]))
                        return std::unexpected("Reference axes must be finite and strictly increasing");
                count *= axis.size();
            }
            result->field_ = json.at("field").get<std::vector<std::array<double, 3>>>();
            if (result->field_.size() != count) return std::unexpected("Reference map has missing grid nodes");
            for (const auto& vector : result->field_)
                for (const auto value : vector)
                    if (!std::isfinite(value)) return std::unexpected("Non-finite reference field value");
            return std::shared_ptr<const ReferenceFieldMap>{std::move(result)};
        } catch (const std::exception& error) {
            return std::unexpected("Invalid reference map: " + std::string{error.what()});
        }
    }

    [[nodiscard]] std::expected<std::array<double, 3>, std::string> evaluate(std::array<double, 3> point) const {
        std::array<std::size_t, 3> cell;
        std::array<double, 3> amount;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const auto& values = axes_[axis];
            if (!std::isfinite(point[axis]) || point[axis] < values.front() || point[axis] > values.back())
                return std::unexpected("Requested position lies outside the reference map");
            const auto upper = std::upper_bound(values.begin(), values.end(), point[axis]);
            cell[axis] = std::min(static_cast<std::size_t>(upper - values.begin() - 1), values.size() - 2);
            amount[axis] = (point[axis] - values[cell[axis]]) / (values[cell[axis] + 1] - values[cell[axis]]);
        }
        std::array<double, 3> result{};
        for (std::size_t corner = 0; corner < 8; ++corner) {
            const auto dx = corner & 1;
            const auto dy = (corner >> 1) & 1;
            const auto dz = (corner >> 2) & 1;
            const auto index = ((cell[2] + dz) * axes_[1].size() + cell[1] + dy) * axes_[0].size() + cell[0] + dx;
            const double weight = (dx ? amount[0] : 1 - amount[0]) * (dy ? amount[1] : 1 - amount[1])
                * (dz ? amount[2] : 1 - amount[2]);
            for (std::size_t component = 0; component < 3; ++component)
                result[component] += weight * field_[index][component];
        }
        return result;
    }

private:
    std::array<std::vector<double>, 3> axes_;
    std::vector<std::array<double, 3>> field_;
};
} // namespace tpc_slint::models
