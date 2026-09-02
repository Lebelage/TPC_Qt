#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QStringView>
#include <QVariant>

#include <algorithm>
#include <cstddef>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tpc_qt::models {
    struct SensorRow {
        QString name;
        double value{0.0};
    };

    class SensorsTableModel final : public QAbstractTableModel {
        Q_OBJECT

    public:
        enum Column : int {
            Name = 0,
            Value,
            ColumnCount
        };
        enum CustomRoles {
            NameRole = Qt::UserRole + 1,
            ValueRole
        };

        explicit SensorsTableModel(QObject *parent = nullptr)
            : QAbstractTableModel(parent) {
        }

        [[nodiscard]]
        QHash<int, QByteArray> roleNames() const override {
            QHash<int, QByteArray> roles = QAbstractTableModel::roleNames();
            roles[NameRole] = "sensorName";
            roles[ValueRole] = "sensorValue";
            return roles;
        }

        static std::tuple<QChar, int, QChar> parse_key(const QString &key) {
            if (key.isEmpty()) {
                return std::make_tuple(QChar(), 0, QChar());
            }

            const QChar side = key.front();
            const QChar axis = key.back();
            int num = 0;

            if (key.size() > 2) {
                num = QStringView(key).mid(1, key.size() - 2).toInt();
            }

            return std::make_tuple(side, num, axis);
        }

        [[nodiscard]]
        int rowCount(const QModelIndex &parent = QModelIndex()) const override {
            if (parent.isValid())
                return 0;

            return static_cast<int>(sensors_.size());
        }

        [[nodiscard]]
        int columnCount(const QModelIndex &parent = QModelIndex()) const override {
            if (parent.isValid())
                return 0;

            return static_cast<int>(ColumnCount);
        }

        [[nodiscard]]
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
            if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(sensors_.size()))
                return {};

            const auto &sensor = sensors_[static_cast<std::size_t>(index.row())];

            // Поддержка стандартных таблиц
            if (role == Qt::DisplayRole || role == Qt::EditRole) {
                switch (index.column()) {
                    case Name: return sensor.name;
                    case Value: return sensor.value;
                    default: break;
                }
            }

            // 3. Поддержка нашего ListView (отдаем данные по запросу QML)
            if (role == NameRole) return sensor.name;
            if (role == ValueRole) return sensor.value;

            return {};
        }

        [[nodiscard]]
        QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
            if (role != Qt::DisplayRole)
                return {};

            if (orientation == Qt::Vertical)
                return section + 1;

            switch (section) {
                case Name: return QStringLiteral("Sensor");
                case Value: return QStringLiteral("Value");
                default: break;
            }

            return {};
        }

        void add_sensor(QString name, double value = 0.0) {
            const std::string name_std = name.toStdString();

            if (name_to_index_.contains(name_std)) {
                set_value(name, value);
                return;
            }

            const auto target_key = parse_key(name);

            auto it = std::lower_bound(
                sensors_.begin(), sensors_.end(), target_key,
                [](const SensorRow &item, const auto &key) {
                    return parse_key(item.name) < key;
                }
            );

            const int row = static_cast<int>(std::distance(sensors_.begin(), it));

            // Уведомляем Qt View о вставке строки именно на позицию 'row'
            beginInsertRows(QModelIndex(), row, row);

            sensors_.insert(it, {std::move(name), value});

            // После вставки элементы сдвинулись — пересчитываем соответствие name -> index
            rebuild_indices();

            endInsertRows();
        }

        bool set_value(const QString &name, double value) {
            auto it = name_to_index_.find(name.toStdString());
            if (it == name_to_index_.end())
                return false;

            const int row = it->second;
            if (sensors_[row].value == value)
                return false;

            sensors_[row].value = value;

            const QModelIndex model_index = index(row, static_cast<int>(Value));
            emit dataChanged(model_index, model_index, {Qt::DisplayRole, Qt::EditRole});
            return true;
        }

        void set_sensors(std::vector<SensorRow> sensors) {
            beginResetModel();

            sensors_ = std::move(sensors);

            std::ranges::sort(sensors_, [](const SensorRow &a, const SensorRow &b) {
                return parse_key(a.name) < parse_key(b.name);
            });

            name_to_index_.clear();
            for (int i = 0; i < static_cast<int>(sensors_.size()); ++i) {
                name_to_index_[sensors_[i].name.toStdString()] = i;
            }

            endResetModel();
        }

        void clear() {
            if (sensors_.empty())
                return;

            beginResetModel();
            sensors_.clear();
            name_to_index_.clear();
            endResetModel();
        }

    private:
        void rebuild_indices() {
            name_to_index_.clear();
            for (int i = 0; i < static_cast<int>(sensors_.size()); ++i) {
                name_to_index_[sensors_[i].name.toStdString()] = i;
            }
        }

    private:
        std::vector<SensorRow> sensors_;
        std::unordered_map<std::string, int> name_to_index_;
    };
} // namespace tpc_qt::models
