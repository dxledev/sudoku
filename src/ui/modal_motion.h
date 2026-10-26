#pragma once

#include <QGraphicsEffect>
#include <QTimeLine>

namespace sudoku {

/** @brief Fade and slight scale effect that animates a modal card in and out. */
class ModalMotion : public QGraphicsEffect {
    Q_OBJECT
public:
    /** @brief Length of one open or close transition in milliseconds. */
    static constexpr int duration = 120;
    /** @brief Create the effect with @p parent as its owner. */
    explicit ModalMotion(QObject *parent = nullptr);
    /** @brief Current progress, 0 when hidden and 1 when fully open. */
    qreal progress() const { return progress_; }
    /** @brief Scale applied to the card at the current progress. */
    qreal scale() const { return .98 + .02 * progress_; }
    /** @brief Animate toward open or closed, continuing from the current progress. */
    void animate(bool open);
    /** @brief Stop any transition and return to the hidden state. */
    void reset();

signals:
    /** @brief Emitted whenever progress changes. */
    void progressChanged(qreal progress);
    /** @brief Emitted when a transition toward @p open completes. */
    void finished(bool open);

protected:
    /** @brief Draw the source scaled around its center with progress as opacity. */
    void draw(QPainter *painter) override;

private:
    QTimeLine timeline_;
    qreal startProgress_ = 0;
    qreal progress_ = 0;
    bool targetOpen_ = false;
    /** @brief Store progress, repaint, and notify listeners. */
    void setProgress(qreal value);
};

}
