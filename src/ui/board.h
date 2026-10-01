#pragma once

#include "core/game.h"
#include "core/theme.h"
#include <QWidget>

namespace sudoku {
class AnimatedTooltip;
class SolvedOverlay;

/** @brief Custom-painted interactive 9 by 9 board widget. */
class Board : public QWidget {
    Q_OBJECT
public:
    /** @brief Create the board and its transient tooltip and solved overlay. */
    explicit Board(QWidget *parent = nullptr);
    /** @brief Attach the current game; the game remains owned by the window. */
    void setGame(Game *game);
    /** @brief Replace the rendering palette and refresh the board. */
    void setTheme(const Theme &theme);
    /** @brief Hide the grid and show the paused presentation when true. */
    void setPaused(bool paused);
    /** @brief Show the loading presentation while a puzzle is generated. */
    void setLoading(bool loading);
    /** @brief Enable or disable incorrect-entry coloring. */
    void setCheckMistakes(bool enabled);
    /** @brief Enable or disable pencil-note input mode. */
    void setPencil(bool enabled);
    /** @brief Update the solved overlay when game completion changes. */
    void refreshCompletion();
    /** @brief Return the selected row-major cell index. */
    int selected() const { return selected_; }
    /** @brief Select a row-major cell index and notify listeners when it changes. */
    void selectCell(int index);
    /** @brief Return the centered square occupied by the rendered grid. */
    QRectF boardRect() const;
    /** @brief Provide a preferred square size for layout negotiation. */
    QSize sizeHint() const override { return {520, 520}; }

signals:
    /** @brief Emitted when keyboard input requests a digit from the owning window. */
    void digitRequested(int digit);
    /** @brief Emitted when keyboard input requests erasing the selection. */
    void eraseRequested();
    /** @brief Emitted when the selected cell changes. */
    void selectionChanged();

protected:
    /** @brief Handle tooltips and transient pointer movement. */
    bool event(QEvent *event) override;
    /** @brief Paint the grid, cell states, notes, or loading and pause covers. */
    void paintEvent(QPaintEvent *) override;
    /** @brief Keep transient overlays aligned with the rendered grid. */
    void resizeEvent(QResizeEvent *event) override;
    /** @brief Convert a pointer press into a selected cell. */
    void mousePressEvent(QMouseEvent *event) override;
    /** @brief Handle digit entry, erasing, and keyboard cell navigation. */
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
    /** @brief Paint one cell background, value, and optional pencil notes. */
    void paintCell(QPainter &painter, int index, const QRectF &rectangle);
    /** @brief Paint the pause or generation cover in the board area. */
    void paintCover(QPainter &painter, const QRectF &rectangle);
    /** @brief Paint all cells and the bold 3 by 3 box boundaries. */
    void paintGrid(QPainter &painter, const QRectF &rectangle);
    /** @brief Render the current grid into a square pixmap for modal overlays. */
    QPixmap gridSnapshot();
};

}
