#include "animated_tooltip.h"

#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <algorithm>

namespace sudoku {

namespace {
constexpr int fadeMilliseconds = 150;
}

AnimatedTooltip::AnimatedTooltip(QWidget *parent)
    : QLabel(parent), opacity_(new QGraphicsOpacityEffect(this)),
      animation_(opacity_, "opacity") {
    setObjectName("boardTooltip");
    setWordWrap(true);
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setGraphicsEffect(opacity_);
    opacity_->setOpacity(0);
    animation_.setDuration(fadeMilliseconds);
    animation_.setEasingCurve(QEasingCurve::InOutQuad);
    connect(&animation_, &QPropertyAnimation::finished, this, [this] {
        if (opacity_->opacity() == 0)
            hide();
    });
    hide();
}

void AnimatedTooltip::animateTo(qreal opacity) {
    animation_.stop();
    animation_.setStartValue(opacity_->opacity());
    animation_.setEndValue(opacity);
    animation_.start();
}

void AnimatedTooltip::paintEvent(QPaintEvent *event) {
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().color(QPalette::Window));
    }
    QLabel::paintEvent(event);
}

void AnimatedTooltip::showAt(const QString &text, const QPoint &position) {
    const auto bounds = parentWidget()->rect();
    setMaximumWidth(bounds.width());
    setText(text);
    adjustSize();
    move(std::clamp(position.x(), 0, std::max(0, bounds.width() - width())),
         std::clamp(position.y() + 20, 0, std::max(0, bounds.height() - height())));
    show();
    raise();
    animateTo(1);
}

void AnimatedTooltip::fadeOut() {
    animateTo(0);
}

}
