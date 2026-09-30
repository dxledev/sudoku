#pragma once

#include "core/game.h"
#include "core/theme.h"
#include <QWidget>

namespace sudoku {
class AnimatedTooltip;
class SolvedOverlay;

class Board : public QWidget {
    Q_OBJECT
public:
    explicit Board(QWidget *parent = nullptr);
    void setGame(Game *game);
    void setTheme(const Theme &theme);
    void setPaused(bool paused);
    void setLoading(bool loading);
    void setCheckMistakes(bool enabled);
    void setPencil(bool enabled);
    void refreshCompletion();
    int selected() const { return selected_; }
    void selectCell(int index);
    QRectF boardRect() const;
    QSize sizeHint() const override { return {520, 520}; }

signals:
    void digitRequested(int digit);
    void eraseRequested();
    void selectionChanged();

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    Game *game_ = nullptr;
    Theme theme_ = presetTheme("forest");
    int selected_ = 0;
    bool paused_ = false;
    bool loading_ = true;
    bool checkMistakes_ = true;
    bool pencil_ = false;
    bool tooltipShown_ = false;
    AnimatedTooltip *tooltip_;
    SolvedOverlay *solvedOverlay_;
    void paintCell(QPainter &painter, int index, const QRectF &rectangle);
    void paintCover(QPainter &painter, const QRectF &rectangle);
    void paintGrid(QPainter &painter, const QRectF &rectangle);
    QPixmap gridSnapshot();
};

}
