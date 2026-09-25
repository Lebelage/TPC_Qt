#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QString>
#include <QVariant>

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "models/tpc_data_model.hpp"

namespace tpc_qt::models {

/**
 * Read-only projection of the domain Sensor model for Qt views.
 *
 * Each row represents one physical sensor (for example W1). R/F/Z values and
 * X/Y/Z coordinates are read directly from models::Sensor instead of being
 * duplicated in a UI-specific row structure.
 */
class SensorsTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column : int {
        Name = 0,
        ComponentR,
        ComponentF,
        ComponentZ,
        CoordinateX,
        CoordinateY,
        CoordinateZ,
        ColumnCount
    };

    enum CustomRole : int {
        NameRole = Qt::UserRole + 1,
        ComponentRRole,
        ComponentFRole,
        ComponentZRole,
        CoordinateXRole,
        CoordinateYRole,
        CoordinateZRole
    };

    explicit SensorsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override {
        auto roles = QAbstractTableModel::roleNames();
        roles[NameRole] = "sensorName";
        roles[ComponentRRole] = "componentR";
        roles[ComponentFRole] = "componentF";
        roles[ComponentZRole] = "componentZ";
        roles[CoordinateXRole] = "coordinateX";
        roles[CoordinateYRole] = "coordinateY";
        roles[CoordinateZRole] = "coordinateZ";
        return roles;
    }

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(sensors_.size());
    }

    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(ColumnCount);
    }

    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
            return {};
        }

        const Sensor& sensor = sensors_[static_cast<std::size_t>(index.row())];
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            return dataForColumn(sensor, static_cast<Column>(index.column()));
        }

        switch (role) {
            case NameRole: return QString::fromStdString(sensor.name.toString());
            case ComponentRRole: return sensor.value(SensorNameComponent::R);
            case ComponentFRole: return sensor.value(SensorNameComponent::F);
            case ComponentZRole: return sensor.value(SensorNameComponent::Z);
            case CoordinateXRole: return sensor.coordinate(SensorCoordinate::X);
            case CoordinateYRole: return sensor.coordinate(SensorCoordinate::Y);
            case CoordinateZRole: return sensor.coordinate(SensorCoordinate::Z);
            default: return {};
        }
    }

    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole
    ) const override {
        if (role != Qt::DisplayRole) {
            return {};
        }
        if (orientation == Qt::Vertical) {
            return section + 1;
        }

        switch (static_cast<Column>(section)) {
            case Name: return QStringLiteral("Sensor");
            case ComponentR: return QStringLiteral("R");
            case ComponentF: return QStringLiteral("F");
            case ComponentZ: return QStringLiteral("Z");
            case CoordinateX: return QStringLiteral("X");
            case CoordinateY: return QStringLiteral("Y");
            case CoordinateZ: return QStringLiteral("Z");
            case ColumnCount: return {};
        }

        return {};
    }

    /** Replaces sensor metadata while preserving already received values. */
    void setSensors(std::vector<Sensor> sensors) {
        for (auto& sensor : sensors) {
            const auto existing = std::ranges::find_if(sensors_, [&sensor](const Sensor& current) {
                return sameName(current.name, sensor.name);
            });
            if (existing != sensors_.end()) {
                sensor.values = existing->values;
            }
        }

        std::ranges::sort(sensors, {}, [](const Sensor& sensor) {
            return std::pair{static_cast<char>(sensor.name.id), sensor.name.number};
        });

        beginResetModel();
        sensors_ = std::move(sensors);
        rebuildIndices();
        endResetModel();
    }

    /** Applies channel keys such as W1R to the matching sensor and component. */
    void applyFrame(const TpcDataModel::ReceivedFrame& frame) {
        for (const auto& [channel_name, value] : frame) {
            const auto sensor_name = SensorName::parse(channel_name);
            const auto component = SensorName::parseComponent(channel_name);
            if (!sensor_name || !component) {
                continue;
            }

            const QString key = QString::fromStdString(sensor_name->toString());
            const auto row_it = name_to_index_.constFind(key);
            if (row_it == name_to_index_.cend()) {
                continue;
            }

            const int row = row_it.value();
            Sensor& sensor = sensors_[static_cast<std::size_t>(row)];
            if (sensor.value(*component) == value) {
                continue;
            }

            sensor.setValue(value, *component);
            const Column column = columnFor(*component);
            const QModelIndex changed = index(row, column);
            Q_EMIT dataChanged(changed, changed, {Qt::DisplayRole, Qt::EditRole, roleFor(*component)});
        }
    }

private:
    [[nodiscard]] static bool sameName(const SensorName& lhs, const SensorName& rhs) noexcept {
        return lhs.id == rhs.id && lhs.number == rhs.number;
    }

    [[nodiscard]] static Column columnFor(SensorNameComponent component) noexcept {
        switch (component) {
            case SensorNameComponent::R: return ComponentR;
            case SensorNameComponent::F: return ComponentF;
            case SensorNameComponent::Z: return ComponentZ;
        }
        return ComponentR;
    }

    [[nodiscard]] static int roleFor(SensorNameComponent component) noexcept {
        switch (component) {
            case SensorNameComponent::R: return ComponentRRole;
            case SensorNameComponent::F: return ComponentFRole;
            case SensorNameComponent::Z: return ComponentZRole;
        }
        return ComponentRRole;
    }

    [[nodiscard]] static QVariant dataForColumn(const Sensor& sensor, Column column) {
        switch (column) {
            case Name: return QString::fromStdString(sensor.name.toString());
            case ComponentR: return sensor.value(SensorNameComponent::R);
            case ComponentF: return sensor.value(SensorNameComponent::F);
            case ComponentZ: return sensor.value(SensorNameComponent::Z);
            case CoordinateX: return sensor.coordinate(SensorCoordinate::X);
            case CoordinateY: return sensor.coordinate(SensorCoordinate::Y);
            case CoordinateZ: return sensor.coordinate(SensorCoordinate::Z);
            case ColumnCount: return {};
        }
        return {};
    }

    void rebuildIndices() {
        name_to_index_.clear();
        for (int row = 0; row < static_cast<int>(sensors_.size()); ++row) {
            name_to_index_.insert(QString::fromStdString(sensors_[row].name.toString()), row);
        }
    }

    std::vector<Sensor> sensors_;
    QHash<QString, int> name_to_index_;
};

}  // namespace tpc_qt::models
