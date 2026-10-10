#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "tpc/analytics/models/three_dimension_models.hpp"
#include "tpc/utilities/header_function.hpp"
namespace tpc::analytics {
class BasisCollection {
public:
    using Function = utilities::header_function<double(std::size_t, double, double, double)>;

    static std::expected<BasisCollection, std::string> create(
        std::size_t arity, std::size_t modes, models::CoordinateType basis_type
    ) {
        if (arity == 0)
            return std::unexpected("Basis arity must be greater than zero");

        if (modes == 0)
            return std::unexpected("Basis modes count must be greater than zero");

        return BasisCollection{arity, modes, basis_type};
    }

    ~BasisCollection() = default;

private:
    BasisCollection(std::size_t arity, std::size_t modes, models::CoordinateType basis_type)
        : arity_{arity}, modes_{modes}, basis_type_{basis_type} {
        basis_.reserve(arity);
    }

public:
    BasisCollection(const BasisCollection&) = delete;
    BasisCollection& operator=(const BasisCollection&) = delete;

    BasisCollection(BasisCollection&&) noexcept = default;
    BasisCollection& operator=(BasisCollection&&) noexcept = default;

    [[nodiscard]] bool empty() const noexcept {
        return basis_.empty();
    }

    std::expected<void, std::string> add_back(Function function) {
        if (basis_.size() >= arity_)
            return std::unexpected("Basis collection is full");

        basis_.emplace_back(std::move(function));
        return {};
    }

    [[nodiscard]] std::size_t get_arity() const noexcept {
        return arity_;
    }

    [[nodiscard]] std::size_t get_modes() const noexcept {
        return modes_;
    }

    [[nodiscard]] models::CoordinateType get_basis_type() const noexcept {
        return basis_type_;
    }

    [[nodiscard]] std::span<const Function> get_basis() const noexcept {
        return basis_;
    }

private:
    std::size_t arity_{};
    std::size_t modes_{};
    models::CoordinateType basis_type_{};

    std::vector<Function> basis_{};
};

}  // namespace tpc::analytics
