module;
#include <vector>
#include <string>
export module tpc_qt.models.ui.initialization_data_model;
export namespace tpc_qt::models {
    struct InitializationData {
        std::vector<std::string> sensors_name;
    };
}
