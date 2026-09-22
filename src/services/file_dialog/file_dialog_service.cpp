
#include "services/file_dialog/file_dialog_service.hpp"

#include <QFileDialog>
#include <QString>
#include <QWidget>

namespace tpc_qt::services {

std::optional<std::filesystem::path>
FileDialogService::open_file(
    QWidget* parent,
    std::string_view title,
    std::string_view filter)
{
    const QString path = QFileDialog::getOpenFileName(
        parent,
        QString::fromUtf8(title.data(), static_cast<qsizetype>(title.size())),
        {},
        QString::fromUtf8(filter.data(), static_cast<qsizetype>(filter.size()))
    );

    if (path.isEmpty()) {
        return std::nullopt;
    }

    return std::filesystem::path{
        path.toStdString()
    };
}


std::optional<std::filesystem::path>
FileDialogService::save_file(
    QWidget* parent,
    std::string_view title,
    std::string_view filter)
{
    const QString path = QFileDialog::getSaveFileName(
        parent,
        QString::fromUtf8(title.data(), static_cast<qsizetype>(title.size())),
        {},
        QString::fromUtf8(filter.data(), static_cast<qsizetype>(filter.size()))
    );

    if (path.isEmpty()) {
        return std::nullopt;
    }

    return std::filesystem::path{
        path.toStdString()
    };
}


std::optional<std::filesystem::path>
FileDialogService::select_directory(
    QWidget* parent,
    std::string_view title)
{
    const QString path = QFileDialog::getExistingDirectory(
        parent,
        QString::fromUtf8(title.data(), static_cast<qsizetype>(title.size()))
    );

    if (path.isEmpty()) {
        return std::nullopt;
    }

    return std::filesystem::path{
        path.toStdString()
    };
}

}