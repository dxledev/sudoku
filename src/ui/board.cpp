#include "board.h"
#include "animated_tooltip.h"
#include "solved_overlay.h"

#include <QKeyEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace sudoku {

Board::Board(QWidget *parent)
    : QWidget(parent), tooltip_(new AnimatedTooltip(this)), solvedOverlay_(new SolvedOverlay(this)) {
    setObjectName("board");
    setMinimumSize(288, 288);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAccessibleName("Sudoku board");
    setToolTip("Arrow keys or hjkl to move · 1–9 to enter · N for notes · Backspace to erase");
}

bool Board::event(QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        if (!tooltipShown_ && !(game_ && game_->complete())) {
            const auto *help = static_cast<QHelpEvent *>(event);
            tooltip_->showAt(toolTip(), help->pos());
            tooltipShown_ = true;
        }
        event->accept();
        return true;
    }
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::Leave) {
        tooltip_->fadeOut();
        if (event->type() == QEvent::Leave)
            tooltipShown_ = false;
    }
    return QWidget::event(event);
}

void Board::setGame(Game *game) {
    solvedOverlay_->clear();
    game_ = game;
    selected_ = 0;
    if (game_) {
        const auto first = std::find(game_->values().begin(), game_->values().end(), 0);
        if (first != game_->values().end())
            selected_ = static_cast<int>(std::distance(game_->values().begin(), first));
    }
    selectCell(selected_);
    refreshCompletion();
}

void Board::setTheme(const Theme &theme) {
    theme_ = theme;
    tooltip_->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid %3; padding: 7px;")
                               .arg(theme.hex("surface_alt"), theme.hex("text"), theme.hex("border")));
    if (!solvedOverlay_->isHidden())
        solvedOverlay_->setSnapshot(gridSnapshot(), theme_);
    update();
}
void Board::setPaused(bool paused) { paused_ = paused; update(); }
void Board::setLoading(bool loading) { loading_ = loading; update(); }
void Board::setCheckMistakes(bool enabled) { checkMistakes_ = enabled; update(); }
void Board::setPencil(bool enabled) { pencil_ = enabled; update(); }

void Board::refreshCompletion() {
    const bool solved = game_ && game_->complete() && !loading_ && !paused_;
    if (solved && solvedOverlay_->isHidden()) {
        tooltip_->hide();
        solvedOverlay_->setGeometry(boardRect().toRect());
        solvedOverlay_->reveal(gridSnapshot(), theme_);
        setAccessibleDescription("Puzzle solved");
    } else if (!solved && !solvedOverlay_->isHidden()) {
        solvedOverlay_->clear();
    }
}

QPixmap Board::gridSnapshot() {
    // Bound the cached blur independently of window size and display scale.
    QPixmap snapshot(384, 384);
    snapshot.fill(theme_.color("surface"));
    QPainter painter(&snapshot);
    painter.setRenderHint(QPainter::Antialiasing);
    paintGrid(painter, QRectF(snapshot.rect()));
    return snapshot;
}

void Board::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    solvedOverlay_->setGeometry(boardRect().toRect());
}

QRectF Board::boardRect() const {
    const qreal side = std::max(0, std::min(width(), height()) - 4);
    return {(width() - side) / 2, (height() - side) / 2, side, side};
}

void Board::paintCell(QPainter &painter, int index, const QRectF &rectangle) {
    const int value = game_->values()[index];
    const int selectedValue = game_->values()[selected_];
    QString background = "surface";
    if (arePeers(index, selected_))
        background = "related";
    if (value && value == selectedValue)
        background = "matching";
    if (index == selected_)
        background = "selection";
    painter.fillRect(rectangle, theme_.color(background));
    QFont cellFont = font();
    if (value) {
        cellFont.setPixelSize(qRound(rectangle.width() * .42));
        cellFont.setWeight(game_->puzzle().clues[index] ? QFont::Medium : QFont::Normal);
        painter.setFont(cellFont);
        const bool highlightMistake = checkMistakes_ && game_->puzzle().difficulty == Difficulty::Easy && game_->wrong(index);
        painter.setPen(theme_.color(highlightMistake ? "error"
                                    : game_->puzzle().clues[index] ? "text" : "accent"));
        painter.drawText(rectangle, Qt::AlignCenter, QString::number(value));
    } else {
        cellFont.setPixelSize(std::max(9, qRound(rectangle.width() * .19)));
        painter.setFont(cellFont);
        painter.setPen(theme_.color("muted"));
        for (int digit = 1; digit <= 9; ++digit) {
            if (!(game_->notes()[index] & (1u << digit)))
                continue;
            QRectF note(rectangle.left() + (digit - 1) % 3 * rectangle.width() / 3,
                        rectangle.top() + (digit - 1) / 3 * rectangle.height() / 3,
                        rectangle.width() / 3, rectangle.height() / 3);
            painter.drawText(note, Qt::AlignCenter, QString::number(digit));
        }
    }
    if (index == selected_) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(theme_.color("accent"), pencil_ ? 1.5 : 2));
        painter.drawRoundedRect(rectangle.adjusted(3, 3, -3, -3), 5, 5);
    }
}

void Board::paintCover(QPainter &painter, const QRectF &rectangle) {
    painter.fillRect(rectangle, theme_.color("surface"));
    const auto center = rectangle.center();
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme_.color("accent"));
    if (paused_) {
        painter.drawRoundedRect(QRectF(center.x() - 13, center.y() - 59, 8, 28), 2, 2);
        painter.drawRoundedRect(QRectF(center.x() + 5, center.y() - 59, 8, 28), 2, 2);
    } else {
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                if (row == 1 && column == 1)
                    continue;
                painter.drawRoundedRect(QRectF(center.x() - 22 + column * 16,
                                               center.y() - 68 + row * 16, 11, 11), 3, 3);
            }
        }
    }
    auto textFont = font();
    textFont.setPixelSize(24);
    textFont.setWeight(QFont::Medium);
    painter.setFont(textFont);
    painter.setPen(theme_.color("text"));
    painter.drawText(rectangle.adjusted(0, 8, 0, 8), Qt::AlignCenter,
                     paused_ ? "Take your time." : "A fresh start.");
    textFont.setPixelSize(13);
    textFont.setWeight(QFont::Normal);
    painter.setFont(textFont);
    painter.setPen(theme_.color("muted"));
    painter.drawText(rectangle.adjusted(0, 69, 0, 69), Qt::AlignCenter,
                     paused_ ? "Press Space or Resume to return" : "Finding your next puzzle…");
}

void Board::paintGrid(QPainter &painter, const QRectF &rectangle) {
    const qreal cell = rectangle.width() / 9;
    for (int index = 0; index < 81; ++index) {
        const QRectF bounds(rectangle.x() + index % 9 * cell,
                            rectangle.y() + index / 9 * cell, cell, cell);
        paintCell(painter, index, bounds);
    }
    for (int line = 1; line < 9; ++line) {
        painter.setPen(QPen(theme_.color(line % 3 ? "border" : "grid"), line % 3 ? .7 : 1.8));
        painter.drawLine(QPointF(rectangle.x() + line * cell, rectangle.top()),
                         QPointF(rectangle.x() + line * cell, rectangle.bottom()));
        painter.drawLine(QPointF(rectangle.left(), rectangle.y() + line * cell),
                         QPointF(rectangle.right(), rectangle.y() + line * cell));
    }
}

void Board::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF rectangle = boardRect();
    QPainterPath clip;
    clip.addRoundedRect(rectangle, 10, 10);
    painter.setClipPath(clip);
    if (!game_ || loading_ || paused_) {
        paintCover(painter, rectangle);
    } else {
        paintGrid(painter, rectangle);
    }
    painter.setClipping(false);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(theme_.color("grid"), 1.4));
    painter.drawRoundedRect(rectangle, 10, 10);
}

void Board::selectCell(int index) {
    selected_ = std::clamp(index, 0, 80);
    QString description = QString("Row %1, column %2").arg(selected_ / 9 + 1).arg(selected_ % 9 + 1);
    if (game_)
        description += game_->values()[selected_] ? QString(", value %1").arg(game_->values()[selected_]) : ", empty";
    setAccessibleDescription(description);
    emit selectionChanged();
    update();
}

void Board::mousePressEvent(QMouseEvent *event) {
    if (loading_ || paused_ || !game_ || game_->complete() || !boardRect().contains(event->position()))
        return;
    const auto rectangle = boardRect();
    const qreal cell = rectangle.width() / 9;
    const int row = std::min(8, int((event->position().y() - rectangle.y()) / cell));
    const int column = std::min(8, int((event->position().x() - rectangle.x()) / cell));
    selectCell(row * 9 + column);
    setFocus();
}

void Board::keyPressEvent(QKeyEvent *event) {
    if (loading_ || paused_ || !game_ || game_->complete())
        return;
    const int key = event->key();
    if (key >= Qt::Key_1 && key <= Qt::Key_9)
        emit digitRequested(key - Qt::Key_0);
    else if (key == Qt::Key_Backspace || key == Qt::Key_Delete || key == Qt::Key_0)
        emit eraseRequested();
    else if (key == Qt::Key_Left || key == Qt::Key_H)
        selectCell(selected_ / 9 * 9 + (selected_ % 9 + 8) % 9);
    else if (key == Qt::Key_Right || key == Qt::Key_L)
        selectCell(selected_ / 9 * 9 + (selected_ % 9 + 1) % 9);
    else if (key == Qt::Key_Up || key == Qt::Key_K)
        selectCell((selected_ + 72) % 81);
    else if (key == Qt::Key_Down || key == Qt::Key_J)
        selectCell((selected_ + 9) % 81);
    else
        QWidget::keyPressEvent(event);
}

}
