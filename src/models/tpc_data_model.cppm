module;
#include <ranges>
#include <unordered_map>
#include <string>
#include <unordered_set>
#include <optional>
export module tpc_qt.models.tpc_data_model;
import tpc.system.models.system_data;
import tpc_qt.models.ui.application_settings_model;
export namespace tpc_qt::models {
    class TpcDataModel {
    public:
        std::optional<const tpc::system::models::DiscoveryResult &> get_discovery_result() {
            if (discovery_result_.nodes.empty())
                return std::nullopt;

            return discovery_result_;
        }

        void set_discovery_result(tpc::system::models::DiscoveryResult discovery_result) {
            if (discovery_result_.nodes.empty())
                return;

            discovery_result_ = discovery_result;
        }

        std::optional<const std::unordered_map<std::string, double> &> get_received_frame() {
            if (received_frame_.empty())
                return std::nullopt;

            return received_frame_;
        }

        void set_received_frame(std::unordered_map<std::string, double> received) {
            if (received_frame_.empty())
                return;

            received_frame_ = received;
        }

        std::optional<std::span<const SensorInfo> > get_sensors_info() {
            if (sensors_info_.empty())
                return std::nullopt;

            return sensors_info_;
        }

        void set_sensors_info(std::vector<SensorInfo> sensors_info) {
            if (sensors_info_.empty())
                return;

            sensors_info_ = sensors_info;
        }

        std::vector<std::string> get_sensors_names() {
            if (!sensors_info_.empty())
                return sensors_info_
                       | std::views::transform(&SensorInfo::group_name)
                       | std::ranges::to<std::vector<std::string> >();

            return discovery_result_.nodes
                   | std::views::values
                   | std::views::transform([](const std::string &s) { return s.substr(0, 2); })
                   | std::ranges::to<std::unordered_set<std::string> >()
                   | std::ranges::to<std::vector<std::string> >();
        }


    private:
        tpc::system::models::DiscoveryResult discovery_result_{};

        std::unordered_map<std::string, double> received_frame_{};

        std::vector<SensorInfo> sensors_info_{};
    };
}
