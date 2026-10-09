#pragma once

#include <string>
namespace tpc::core::definitions {

inline constexpr std::string_view CLIENT_ERROR = "Client error";
inline constexpr std::string_view CLIENT_WARNING = "Client warning";
inline constexpr std::string_view CLIENT_INFO = "Client info";
inline constexpr std::string_view CLIENT_SUCCESS = "Client success";

}  // namespace tpc::core::definitions
