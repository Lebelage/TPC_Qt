#include "viewmodel/FieldVisualizationViewModel.hpp"

#include <QMetaObject>

#include <algorithm>
#include <cmath>

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

int FieldVisualizationViewModel::sliceIndex() const noexcept { return slice_index_; }
int FieldVisualizationViewModel::sliceCount() const noexcept { return static_cast<int>(axis_coordinates_.size()); }
bool FieldVisualizationViewModel::available() const noexcept { return available_; }
bool FieldVisualizationViewModel::loading() const noexcept { return loading_; }
double FieldVisualizationViewModel::minimumValue() const noexcept { return minimum_value_; }
double FieldVisualizationViewModel::maximumValue() const noexcept { return maximum_value_; }
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
    if (axis_coordinates_.empty()) {
        return QStringLiteral("—");
    }
    return QStringLiteral("%1 = %2").arg(axis()).arg(axis_coordinates_[slice_index_], 0, 'g', 5);
}

void FieldVisualizationViewModel::setAxis(const QString& axis_name) {
    const int new_axis = axis_name.compare(QStringLiteral("X"), Qt::CaseInsensitive) == 0 ? 0
        : axis_name.compare(QStringLiteral("Y"), Qt::CaseInsensitive) == 0 ? 1 : 2;
    if (new_axis == axis_index_) {
        return;
    }
    axis_index_ = new_axis;
    rebuildAxisCoordinates();
}

void FieldVisualizationViewModel::setSliceIndex(int index) {
    if (axis_coordinates_.empty()) {
        return;
    }
    index = std::clamp(index, 0, static_cast<int>(axis_coordinates_.size()) - 1);
    if (slice_index_ == index) {
        return;
    }
    slice_index_ = index;
    requestCurrentSlice();
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
    position[static_cast<std::size_t>(axis_index_)] = axis_coordinates_[static_cast<std::size_t>(slice_index_)];
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
    rebuildAxisCoordinates();
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

void FieldVisualizationViewModel::rebuildAxisCoordinates() {
    axis_coordinates_.clear();
    if (available_) {
        const std::size_t count = std::max<std::size_t>(2, geometry_.grid[static_cast<std::size_t>(axis_index_)]);
        const double extent = axis_index_ == 2 ? geometry_.length * 0.5 : geometry_.radius;
        axis_coordinates_.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            axis_coordinates_.push_back(
                -extent + 2.0 * extent * static_cast<double>(index) / static_cast<double>(count - 1)
            );
        }
    }
    slice_index_ = axis_coordinates_.empty() ? 0 : static_cast<int>(axis_coordinates_.size() / 2);
    requestCurrentSlice();
}

void FieldVisualizationViewModel::requestCurrentSlice() {
    if (!available_ || axis_coordinates_.empty()) {
        return;
    }
    if (!loading_) {
        loading_ = true;
        Q_EMIT loadingChanged();
    }
    Q_EMIT sliceChanged();
    field_slices_.requestSlice(
        axis_index_,
        axis_coordinates_[static_cast<std::size_t>(slice_index_)],
        viewport_width_,
        viewport_height_
    );
}

}  // namespace tpc_qt::view_models
