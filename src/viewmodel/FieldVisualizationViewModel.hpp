#pragma once

#include <QObject>
#include <QImage>
#include <QVariantMap>

#include <array>
#include <memory>

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
    Q_PROPERTY(double slicePosition READ slicePosition NOTIFY sliceChanged)
    Q_PROPERTY(double minimumPosition READ minimumPosition NOTIFY sliceChanged)
    Q_PROPERTY(double maximumPosition READ maximumPosition NOTIFY sliceChanged)
    Q_PROPERTY(double positionStep READ positionStep NOTIFY sliceChanged)
    Q_PROPERTY(QString positionLabel READ positionLabel NOTIFY sliceChanged)
    Q_PROPERTY(double minimumValue READ minimumValue NOTIFY sliceChanged)
    Q_PROPERTY(double maximumValue READ maximumValue NOTIFY sliceChanged)
    Q_PROPERTY(double inductionInterval READ inductionInterval NOTIFY sliceChanged)
    Q_PROPERTY(int inductionBandCount READ inductionBandCount CONSTANT)
    Q_PROPERTY(double imageAspectRatio READ imageAspectRatio NOTIFY imageChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    explicit FieldVisualizationViewModel(services::FieldSliceService& field_slices, QObject* parent = nullptr);

    [[nodiscard]] QString axis() const;
    [[nodiscard]] double slicePosition() const noexcept;
    [[nodiscard]] double minimumPosition() const noexcept;
    [[nodiscard]] double maximumPosition() const noexcept;
    [[nodiscard]] double positionStep() const noexcept;
    [[nodiscard]] QString positionLabel() const;
    [[nodiscard]] double minimumValue() const noexcept;
    [[nodiscard]] double maximumValue() const noexcept;
    [[nodiscard]] double inductionInterval() const noexcept;
    [[nodiscard]] int inductionBandCount() const noexcept;
    [[nodiscard]] double imageAspectRatio() const noexcept;
    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] bool loading() const noexcept;
    [[nodiscard]] QImage sliceImage() const;

    Q_INVOKABLE void setAxis(const QString& axis);
    Q_INVOKABLE bool setSlicePosition(double position);
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
    void rebuildSliceRange();
    void requestCurrentSlice();

    services::FieldSliceService& field_slices_;
    std::shared_ptr<const services::RenderedFieldSlice> rendered_slice_;
    models::FieldGeometry geometry_;
    int axis_index_{2};
    int viewport_width_{640};
    int viewport_height_{520};
    double slice_position_{0.0};
    double minimum_position_{0.0};
    double maximum_position_{0.0};
    double minimum_value_{0.0};
    double maximum_value_{0.0};
    bool available_{false};
    bool loading_{false};
    services::ScopedSubscription<models::FieldGeometry> field_subscription_;
    services::ScopedSubscription<std::shared_ptr<const services::RenderedFieldSlice>> slice_subscription_;
};

}  // namespace tpc_qt::view_models
