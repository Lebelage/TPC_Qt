#pragma once

#include <cmath>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#include "tpc/core/definitions/analytics_definitions.hpp"
#include "tpc/utilities/chunkview.hpp"
namespace tpc::analytics::models {

enum class CoordinateType { Cartesian, Cylindric };

enum class SliceDirection { X, Y, Z };

struct PointComponents {
    std::array<double, core::definitions::DIMENSION> components;
    CoordinateType coordinate_type;

    [[nodiscard]] std::span<const double> get_point_components() const noexcept {
        return components;
    }
    [[nodiscard]] std::span<double> get_point_components() noexcept {
        return components;
    }

    std::expected<void, std::string> transform_coordinates(CoordinateType target_type) {
        if (target_type == coordinate_type) {
            return {};
        }

        if (target_type == CoordinateType::Cylindric) {
            const double x = components[0];
            const double y = components[1];

            components[0] = std::hypot(x, y);
            components[1] = std::atan2(y, x);

        } else if (target_type == CoordinateType::Cartesian) {
            const double r = components[0];
            const double phi = components[1];

            components[0] = r * std::cos(phi);
            components[1] = r * std::sin(phi);
        } else {
            return std::unexpected{"Unsupported coordinate type"};
        }

        coordinate_type = target_type;
        return {};
    }
};

struct FieldComponents {
    std::array<double, core::definitions::DIMENSION> components;
    CoordinateType coordinate_type;

    [[nodiscard]] std::span<const double> get_field_components() const noexcept {
        return components;
    }
    [[nodiscard]] std::span<double> get_field_components() noexcept {
        return components;
    }
};

struct Measurement {
    PointComponents point_components{};
    FieldComponents field_components{};

    [[nodiscard]] std::span<const double> get_point_components() const noexcept {
        return point_components.get_point_components();
    }
    [[nodiscard]] std::span<const double> get_field_components() const noexcept {
        return field_components.get_field_components();
    }
};

/** Numerically evaluated regular 2D slice. Field components are Cartesian. */
struct FieldSlice {
    SliceDirection direction{SliceDirection::Z};
    double coordinate{};
    std::array<std::size_t, 2> grid{};
    std::array<double, 2> horizontal_bounds{};
    std::array<double, 2> vertical_bounds{};
    std::vector<double> field;
    std::vector<std::uint8_t> valid;
};

class FieldCollection {
public:
    FieldCollection(std::size_t dimension, CoordinateType coordinates_type, CoordinateType field_type)
        : dimension_(dimension), coordinates_type_(coordinates_type), field_type_(field_type) {}

    ~FieldCollection() = default;

public:
    void reserve(std::size_t capacity) {
        coordinates_.reserve(capacity);
        field_.reserve(capacity);
    }

    void clear() noexcept {
        coordinates_.clear();
        field_.clear();
    }

    void append_coordinate(double first, double second, double third) {
        coordinates_.push_back(first);
        coordinates_.push_back(second);
        coordinates_.push_back(third);
    }

    void resize_field_to_coordinates() {
        field_.resize(coordinates_.size());
    }

    std::expected<void, std::string> insert_coordinates_range(std::span<const double> coordinates_data) {
        if (coordinates_data.empty())
            return std::unexpected("Coordinates data is empty");

        if ((coordinates_data.size() % dimension_) != 0)
            return std::unexpected("Buffer size is not divisible by its dimension");

        coordinates_.insert(coordinates_.end(), coordinates_data.begin(), coordinates_data.end());

        return {};
    }

    std::expected<void, std::string> insert_field_range(std::span<const double> field_data) {
        if (field_data.empty())
            return std::unexpected("Field data is empty");

        if ((field_data.size() % dimension_) != 0)
            return std::unexpected("Buffer size is not divisible by its dimension");

        field_.insert(field_.end(), field_data.begin(), field_data.end());

        return {};
    }

    std::expected<void, std::string> transform_coordinates(CoordinateType target_type) {
        if (coordinates_.empty())
            return std::unexpected("Coordinates collection is empty");

        if (target_type == coordinates_type_)
            return std::unexpected("Can`t transform coordinates");

        auto chunks = tpc::utilities::ChunkView{std::span<double>(coordinates_), dimension_};

        if (target_type == CoordinateType::Cylindric) {
            std::ranges::for_each(chunks, [](std::span<double> point) {
                const double x = point[0];
                const double y = point[1];

                point[0] = std::hypot(x, y);
                point[1] = std::atan2(y, x);
            });
        } else if (target_type == CoordinateType::Cartesian) {
            std::ranges::for_each(chunks, [](std::span<double> point) {
                const double r = point[0];
                const double phi = point[1];

                point[0] = r * std::cos(phi);
                point[1] = r * std::sin(phi);
            });
        }

        coordinates_type_ = target_type;
        return {};
    }

    std::expected<void, std::string> transform_field(CoordinateType target_type) {
        if (field_.empty())
            return std::unexpected("Field collection is empty");

        if (field_type_ == target_type)
            return {};

        if (dimension_ != 3)
            return std::unexpected("Only 3D fields are supported");

        if (field_type_ != CoordinateType::Cylindric || target_type != CoordinateType::Cartesian) {
            return std::unexpected("Unsupported field transformation");
        }

        if (coordinates_type_ != CoordinateType::Cylindric)
            return std::unexpected("Coordinates must be cylindrical");

        if (coordinates_.size() != field_.size())
            return std::unexpected("Coordinates and field sizes differ");

        for (std::size_t offset = 0; offset < field_.size(); offset += dimension_) {
            const double phi = coordinates_[offset + 1];
            const double br = field_[offset];
            const double bphi = field_[offset + 1];

            field_[offset] = br * std::cos(phi) - bphi * std::sin(phi);
            field_[offset + 1] = br * std::sin(phi) + bphi * std::cos(phi);
        }

        field_type_ = CoordinateType::Cartesian;
        return {};
    }

public:
    [[nodiscard]] std::span<const double> get_field() const noexcept {
        return field_;
    }
    [[nodiscard]] std::span<double> get_mutable_field() noexcept {
        return field_;
    }
    [[nodiscard]] std::span<const double> get_coordinates() const noexcept {
        return coordinates_;
    }

private:
    std::size_t dimension_{};

    CoordinateType coordinates_type_{};
    CoordinateType field_type_{};

    std::vector<double> coordinates_{};
    std::vector<double> field_{};
};
}  // namespace tpc::analytics::models
