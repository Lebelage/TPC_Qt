#include "view/FieldSliceItem.hpp"

#include <QPainter>

#include "viewmodel/FieldVisualizationViewModel.hpp"

namespace tpc_qt::views {

FieldSliceItem::FieldSliceItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setAntialiasing(false);
    setOpaquePainting(false);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
}

QObject* FieldSliceItem::model() const noexcept {
    return model_;
}

void FieldSliceItem::setModel(QObject* model) {
    auto* typed_model = qobject_cast<view_models::FieldVisualizationViewModel*>(model);
    if (model_ == typed_model) {
        return;
    }
    QObject::disconnect(image_connection_);
    model_ = typed_model;
    if (model_) {
        image_connection_ = QObject::connect(
            model_, &view_models::FieldVisualizationViewModel::imageChanged,
            this, [this] { update(); }
        );
    }
    update();
    Q_EMIT modelChanged();
}

void FieldSliceItem::paint(QPainter* painter) {
    if (!model_) {
        return;
    }
    const QImage image = model_->sliceImage();
    if (image.isNull()) {
        return;
    }
    const QSizeF fitted = image.size().scaled(boundingRect().size().toSize(), Qt::KeepAspectRatio);
    const QRectF target{
        (width() - fitted.width()) * 0.5,
        (height() - fitted.height()) * 0.5,
        fitted.width(),
        fitted.height()
    };
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawImage(target, image);
}

}  // namespace tpc_qt::views
