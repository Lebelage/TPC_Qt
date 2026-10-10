#pragma once

#include <concepts>
#include <span>
namespace tpc::analytics::concepts
{
    template <typename T>
    concept MeasurementConcept = requires(const T& m) {
        { m.get_point_components() } -> std::convertible_to<std::span<const double>>;
        { m.get_field_components() } -> std::convertible_to<std::span<const double>>;
    };
}
