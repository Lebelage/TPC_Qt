#include "services/file_dialog/file_dialog_service.hpp"

#include <QFileDialog>
#include <QString>

namespace tpc_qt::services {

std::optional<std::filesystem::path> FileDialogService::saveFile(
    QWidget* parent,
    std::string_view title,
    std::string_view filter
) {
    const QString path = QFileDialog::getSaveFileName(
        parent,
        QString::fromUtf8(title.data(), static_cast<qsizetype>(title.size())),
        {},
        QString::fromUtf8(filter.data(), static_cast<qsizetype>(filter.size()))
    );

    if (path.isEmpty()) {
        return std::nullopt;
    }

    return std::filesystem::path{path.toStdString()};
}

}  // namespace tpc_qt::services
