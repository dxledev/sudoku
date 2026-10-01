#pragma once

#include <QLabel>
#include <QPropertyAnimation>

class QGraphicsOpacityEffect;

namespace sudoku {

/** @brief Small board tooltip that fades in and out over the selected cell. */
class AnimatedTooltip : public QLabel {
public:
    /** @brief Create a tooltip child attached to a board or similar widget. */
    explicit AnimatedTooltip(QWidget *parent);
    /** @brief Show the message near a widget-local position and animate opacity. */
    void showAt(const QString &text, const QPoint &position);
    /** @brief Fade the tooltip away and hide it when the animation completes. */
    void fadeOut();

protected:
    /** @brief Paint the label with the current animated opacity. */
    void paintEvent(QPaintEvent *event) override;

private:
    QGraphicsOpacityEffect *opacity_;
    QPropertyAnimation animation_;
    /** @brief Animate the opacity effect toward a target value. */
    void animateTo(qreal opacity);
};

}
