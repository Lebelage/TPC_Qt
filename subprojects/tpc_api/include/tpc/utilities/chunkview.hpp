#pragma once

#include <algorithm>
#include <span>
#include <stdexcept>

namespace tpc::utilities {
    template<typename T>
    class ChunkView {
    public:
        ChunkView(std::span<T> data, std::size_t chunk_size) : data_{data}, chunk_size_{chunk_size} {
            if (chunk_size_ == 0)
                throw std::invalid_argument("Chunk size must be non-zero");
        }

        class Iterator {
        public:
            using value_type = std::span<T>;
            using difference_type = std::ptrdiff_t;
            using iterator_concept = std::forward_iterator_tag;

            Iterator() = default;

            Iterator(T *current, T *end, std::size_t chunk_size) : current_{current}, end_{end},
                                                                   chunk_size_{chunk_size} {
            }

            [[nodiscard]] std::span<T> operator*() const {
                const auto remaining = static_cast<std::size_t>(end_ - current_);

                return {current_, std::min(remaining, chunk_size_)};
            }

            [[nodiscard]] Iterator &operator++() {
                current_ += std::min(
                    chunk_size_,
                    static_cast<std::size_t>(end_ - current_));

                return *this;
            }

            Iterator operator++(int) {
                auto copy = *this;
                ++(*this);
                return copy;
            }

            friend bool operator==(const Iterator &, const Iterator &) = default;

        private:
            T *current_{};
            T *end_{};
            std::size_t chunk_size_{};
        };

        [[nodiscard]] Iterator begin() const {
            return {data_.data(), data_.data() + data_.size(), chunk_size_};
        }

        [[nodiscard]] Iterator end() const {
            return {
                data_.data() + data_.size(),
                data_.data() + data_.size(),
                chunk_size_
            };
        }

    private:
        std::span<T> data_;
        std::size_t chunk_size_;
    };

    template<typename T>
    ChunkView(std::span<T>, std::size_t) -> ChunkView<T>;
}
