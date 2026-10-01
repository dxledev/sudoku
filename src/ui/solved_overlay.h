#pragma once

#include "core/theme.h"
#include <QPixmap>
#include <QPropertyAnimation>
#include <QWidget>

namespace sudoku {

/** @brief Animated solved state drawn over a blurred board snapshot. */
class SolvedOverlay : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress WRITE setProgress)
public:
    /** @brief Create a transparent overlay parented to the board. */
    explicit SolvedOverlay(QWidget *parent = nullptr);
    /** @brief Capture the solved board palette and animate the reveal. */
    void reveal(const QPixmap &grid, const Theme &theme);
    /** @brief Replace the snapshot and palette without starting the animation. */
    void setSnapshot(const QPixmap &grid, const Theme &theme);
    /** @brief Clear the captured image and hide the overlay. */
    void clear();
    /** @brief Read the animation progress in the range 0 to 1. */
    qreal progress() const { return progress_; }
    /** @brief Set animation progress and schedule a repaint. */
    void setProgress(qreal progress);

protected:
    /** @brief Paint the blurred board and animated solved message. */
    void paintEvent(QPaintEvent *) override;

private:
    Theme theme_;
    QPixmap blurredGrid_;
    QPropertyAnimation animation_;
    qreal progress_ = 0;
};

}
