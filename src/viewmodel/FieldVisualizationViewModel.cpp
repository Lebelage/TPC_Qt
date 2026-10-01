  #include "viewmodel/FieldVisualizationViewModel.hpp"

#include <QMetaObject>

#include <algorithm>
#include <cmath>
#include <limits>

#include "services/field_slice/field_slice_service.hpp"

namespace tpc_qt::view_models {

FieldVisualizationViewModel::FieldVisualizationViewModel(
    services::FieldSliceService& field_slices,
    QObject* parent
) : QObject(parent), field_slices_(field_slices) {
    field_subscription_.subscribe(
        field_slices.field_available,
        [this](models::FieldGeometry geometry) {
            QMetaObject::invokeMethod(this, [this, geometry] {
                acceptGeometry(geometry);
            });
        }
    );
    slice_subscription_.subscribe(
        field_slices.slice_rendered,
        [this](std::shared_ptr<const services::RenderedFieldSlice> slice) {
            QMetaObject::invokeMethod(this, [this, slice = std::move(slice)] {
                acceptSlice(std::move(slice));
            });
        }
    );
}

QString FieldVisualizationViewModel::axis() const {
    static constexpr std::array names{"X", "Y", "Z"};
    return names[static_cast<std::size_t>(axis_index_)];
}

double FieldVisualizationViewModel::slicePosition() const noexcept { return slice_position_; }
double FieldVisualizationViewModel::minimumPosition() const noexcept { return minimum_position_; }
double FieldVisualizationViewModel::maximumPosition() const noexcept { return maximum_position_; }
double FieldVisualizationViewModel::positionStep() const noexcept {
    static constexpr double slider_divisions = 500.0;
    return (maximum_position_ - minimum_position_) / slider_divisions;
}
bool FieldVisualizationViewModel::available() const noexcept { return available_; }
bool FieldVisualizationViewModel::loading() const noexcept { return loading_; }
double FieldVisualizationViewModel::minimumValue() const noexcept { return minimum_value_; }
double FieldVisualizationViewModel::maximumValue() const noexcept { return maximum_value_; }
double FieldVisualizationViewModel::inductionInterval() const noexcept {
    return (maximum_value_ - minimum_value_) / static_cast<double>(inductionBandCount());
}
int FieldVisualizationViewModel::inductionBandCount() const noexcept {
    return static_cast<int>(services::RenderedFieldSlice::induction_band_count);
}
double FieldVisualizationViewModel::imageAspectRatio() const noexcept {
    if (!rendered_slice_ || rendered_slice_->image.height() == 0) {
        return 1.0;
    }
    return static_cast<double>(rendered_slice_->image.width())
        / static_cast<double>(rendered_slice_->image.height());
}

QImage FieldVisualizationViewModel::sliceImage() const {
    return rendered_slice_ ? rendered_slice_->image : QImage{};
}

QString FieldVisualizationViewModel::positionLabel() const {
    if (!available_) {
        return QStringLiteral("—");
    }
    return QStringLiteral("%1 = %2 cm").arg(axis()).arg(slice_position_, 0, 'g', 7);
}

void FieldVisualizationViewModel::setAxis(const QString& axis_name) {
    const int new_axis = axis_name.compare(QStringLiteral("X"), Qt::CaseInsensitive) == 0 ? 0
        : axis_name.compare(QStringLiteral("Y"), Qt::CaseInsensitive) == 0 ? 1 : 2;
    if (new_axis == axis_index_) {
        return;
    }
    axis_index_ = new_axis;
    rebuildSliceRange();
}

bool FieldVisualizationViewModel::setSlicePosition(double position) {
    if (!available_ || !std::isfinite(position)
        || position < minimum_position_ || position > maximum_position_) {
        return false;
    }
    if (std::abs(slice_position_ - position)
        <= std::numeric_limits<double>::epsilon() * std::max(1.0, std::abs(position))) {
        return true;
    }
    slice_position_ = position;
    requestCurrentSlice();
    return true;
}

QVariantMap FieldVisualizationViewModel::sampleAt(double u, double v) const {
    if (!rendered_slice_ || rendered_slice_->image.isNull() || u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0) {
        return {};
    }
    const int width = rendered_slice_->image.width();
    const int height = rendered_slice_->image.height();
    const int x = std::clamp(static_cast<int>(std::round(u * (width - 1))), 0, width - 1);
    const int y = std::clamp(static_cast<int>(std::round(v * (height - 1))), 0, height - 1);
    const auto offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(x);
    if (!rendered_slice_->valid[offset]) {
        return {};
    }

    const std::size_t field_offset = offset * 3;
    const auto& field = rendered_slice_->field;
    const double projected_u = std::lerp(rendered_slice_->u_range[0], rendered_slice_->u_range[1], u);
    const double projected_v = std::lerp(rendered_slice_->v_range[1], rendered_slice_->v_range[0], v);
    std::array<double, 3> position{};
    position[static_cast<std::size_t>(axis_index_)] = slice_position_;
    position[static_cast<std::size_t>(axis_index_ == 0 ? 1 : 0)] = projected_u;
    position[static_cast<std::size_t>(axis_index_ == 2 ? 1 : 2)] = projected_v;
    return {
        {QStringLiteral("x"), position[0]},
        {QStringLiteral("y"), position[1]},
        {QStringLiteral("z"), position[2]},
        {QStringLiteral("bx"), field[field_offset]},
        {QStringLiteral("by"), field[field_offset + 1]},
        {QStringLiteral("bz"), field[field_offset + 2]},
        {QStringLiteral("value"), std::hypot(
            field[field_offset], field[field_offset + 1], field[field_offset + 2]
        )}
    };
}

void FieldVisualizationViewModel::setViewportSize(int width, int height) {
    width = std::clamp(width, 128, 1024);
    height = std::clamp(height, 128, 1024);
    if (std::abs(width - viewport_width_) < 64 && std::abs(height - viewport_height_) < 64) {
        return;
    }
    viewport_width_ = width;
    viewport_height_ = height;
    requestCurrentSlice();
}

void FieldVisualizationViewModel::acceptGeometry(models::FieldGeometry geometry) {
    const bool was_available = available_;
    geometry_ = geometry;
    available_ = geometry_.radius > 0.0 && geometry_.length > 0.0;
    rebuildSliceRange();
    if (was_available != available_) {
        Q_EMIT availableChanged();
    }
}

void FieldVisualizationViewModel::acceptSlice(
    std::shared_ptr<const services::RenderedFieldSlice> slice
) {
    rendered_slice_ = std::move(slice);
    minimum_value_ = rendered_slice_->minimum_value;
    maximum_value_ = rendered_slice_->maximum_value;
    if (loading_) {
        loading_ = false;
        Q_EMIT loadingChanged();
    }
    Q_EMIT sliceChanged();
    Q_EMIT imageChanged();
}

void FieldVisualizationViewModel::rebuildSliceRange() {
    const double extent = available_
        ? (axis_index_ == 2 ? geometry_.length * 0.5 : geometry_.radius)
        : 0.0;
    minimum_position_ = -extent;
    maximum_position_ = extent;
    slice_position_ = 0.0;
    requestCurrentSlice();
}

void FieldVisualizationViewModel::requestCurrentSlice() {
    if (!available_) {
        return;
    }
    if (!loading_) {
        loading_ = true;
        Q_EMIT loadingChanged();
    }
    Q_EMIT sliceChanged();
    field_slices_.requestSlice(
        axis_index_,
        slice_position_,
        viewport_width_,
        viewport_height_
    );
}

}  // namespace tpc_qt::view_models
