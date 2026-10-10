#pragma once

#include <functional>
#include <span>
#include <stdexcept>
namespace tpc::utilities
{
    template <typename Signature>
    class header_function;

    template <typename R, typename Header, typename... BodyArgs>
        requires(sizeof...(BodyArgs) <= 3)
    class header_function<R(Header, BodyArgs...)>
    {
    private:
        std::function<R(Header, BodyArgs...)> function_;

    public:
        header_function() noexcept = default;

        header_function(const header_function&)     = default;
        header_function(header_function&&) noexcept = default;

        header_function& operator=(const header_function&)     = default;
        header_function& operator=(header_function&&) noexcept = default;

        template <typename Callable>
            requires(!std::is_same_v<std::remove_cvref_t<Callable>, header_function>
                     && std::is_invocable_r_v<R, Callable, Header, BodyArgs...>)
        header_function(Callable&& c) : function_(std::forward<Callable>(c))
        {
        }

        static constexpr std::size_t body_size() noexcept { return sizeof...(BodyArgs); }

        R operator()(Header header, BodyArgs... body_args) const
        {
            if (!function_)
            {
                throw std::bad_function_call();
            }
            return function_(header, body_args...);
        }

        template <typename T>
        R invoke_from_span(Header header, std::span<const T> body_values) const
        {
            if (body_values.size() != body_size())
                throw std::invalid_argument("The array size does not match the number of body parameters!");

            return invoke_impl(header, body_values, std::index_sequence_for<BodyArgs...>{});
        }

        explicit operator bool() const noexcept { return static_cast<bool>(function_); }

    private:
        template <typename T, std::size_t... Is>
        R invoke_impl(Header header, std::span<const T> body_values, std::index_sequence<Is...>) const
        {
            if (!function_)
            {
                throw std::bad_function_call();
            }
            return function_(header, body_values[Is]...);
        }
    };
}  // namespace tpc::utilities
