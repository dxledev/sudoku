#pragma once

#include <QLabel>
#include <QPropertyAnimation>

class QGraphicsOpacityEffect;

namespace sudoku {

class AnimatedTooltip : public QLabel {
public:
    explicit AnimatedTooltip(QWidget *parent);
    void showAt(const QString &text, const QPoint &position);
    void fadeOut();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QGraphicsOpacityEffect *opacity_;
    QPropertyAnimation animation_;
    void animateTo(qreal opacity);
};

}
