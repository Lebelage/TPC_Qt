#pragma once

#include <QObject>
#include <QImage>
#include <QVariantMap>

#include <array>
#include <memory>
#include <vector>

#include "models/field_slice_model.hpp"
#include "services/scoped_subscription.hpp"

namespace tpc_qt::services {
class FieldSliceService;
struct RenderedFieldSlice;
}

namespace tpc_qt::view_models {

class FieldVisualizationViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString axis READ axis NOTIFY sliceChanged)
    Q_PROPERTY(int sliceIndex READ sliceIndex NOTIFY sliceChanged)
    Q_PROPERTY(int sliceCount READ sliceCount NOTIFY sliceChanged)
    Q_PROPERTY(QString positionLabel READ positionLabel NOTIFY sliceChanged)
    Q_PROPERTY(double minimumValue READ minimumValue NOTIFY sliceChanged)
    Q_PROPERTY(double maximumValue READ maximumValue NOTIFY sliceChanged)
    Q_PROPERTY(double imageAspectRatio READ imageAspectRatio NOTIFY imageChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    explicit FieldVisualizationViewModel(services::FieldSliceService& field_slices, QObject* parent = nullptr);

    [[nodiscard]] QString axis() const;
    [[nodiscard]] int sliceIndex() const noexcept;
    [[nodiscard]] int sliceCount() const noexcept;
    [[nodiscard]] QString positionLabel() const;
    [[nodiscard]] double minimumValue() const noexcept;
    [[nodiscard]] double maximumValue() const noexcept;
    [[nodiscard]] double imageAspectRatio() const noexcept;
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] bool loading() const noexcept;
    [[nodiscard]] QImage sliceImage() const;

    Q_INVOKABLE void setAxis(const QString& axis);
    Q_INVOKABLE void setSliceIndex(int index);
    Q_INVOKABLE void setViewportSize(int width, int height);
    Q_INVOKABLE QVariantMap sampleAt(double u, double v) const;

Q_SIGNALS:
    void sliceChanged();
    void imageChanged();
    void availableChanged();
    void loadingChanged();

private:
    void acceptGeometry(models::FieldGeometry geometry);
    void acceptSlice(std::shared_ptr<const services::RenderedFieldSlice> slice);
    void rebuildAxisCoordinates();
    void requestCurrentSlice();

    services::FieldSliceService& field_slices_;
    std::shared_ptr<const services::RenderedFieldSlice> rendered_slice_;
    models::FieldGeometry geometry_;
    std::vector<double> axis_coordinates_;
    int axis_index_{2};
    int slice_index_{0};
    int viewport_width_{640};
    int viewport_height_{520};
    double minimum_value_{0.0};
    double maximum_value_{0.0};
    bool available_{false};
    bool loading_{false};
    services::ScopedSubscription<models::FieldGeometry> field_subscription_;
    services::ScopedSubscription<std::shared_ptr<const services::RenderedFieldSlice>> slice_subscription_;
};

}  // namespace tpc_qt::view_models
