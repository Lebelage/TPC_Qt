#pragma once

#include <iostream>
#include <string>

namespace tpc::core
{
    enum class LogType
    {
        Error,
        Warning,
        Success,
        Info
    };

    class Logger
    {
    public:
        static void Log(LogType log_type, std::string_view text)
        {
            std::string type{};

            switch (log_type)
            {
                case LogType::Error:
                    type = "Error";
                    break;
                case LogType::Warning:
                    type = "Warning";
                    break;
                case LogType::Success:
                    type = "Success";
                    break;
                case LogType::Info:
                    type = "Info";
                    break;
            }

            auto formatted_str = std::format("[{}]: {}", type, text);

            std::cout << formatted_str << "\n";
        }
    };
}  // namespace tpc::core
