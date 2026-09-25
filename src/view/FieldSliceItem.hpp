#pragma once

#include <QMetaObject>
#include <QQuickPaintedItem>

namespace tpc_qt::view_models {
class FieldVisualizationViewModel;
}

namespace tpc_qt::views {

/** Lightweight scene-graph item that uploads an already rendered field slice. */
class FieldSliceItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QObject* model READ model WRITE setModel NOTIFY modelChanged)

public:
    explicit FieldSliceItem(QQuickItem* parent = nullptr);

    [[nodiscard]] QObject* model() const noexcept;
    void setModel(QObject* model);
    void paint(QPainter* painter) override;

Q_SIGNALS:
    void modelChanged();

private:
    view_models::FieldVisualizationViewModel* model_{nullptr};
    QMetaObject::Connection image_connection_;
};

}  // namespace tpc_qt::views
