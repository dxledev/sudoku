#pragma once

#include "core/puzzle.h"
#include "core/theme.h"
#include <QWidget>

class QPushButton;

namespace sudoku {

class NewPuzzleModal : public QWidget {
    Q_OBJECT
public:
    explicit NewPuzzleModal(QWidget *parent);
    void showConfirmation(Difficulty difficulty);
    void setTheme(const Theme &theme);

signals:
    void confirmed(Difficulty difficulty);
    void dismissed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QColor backdrop_;
    QWidget *card_;
    QPushButton *keepButton_, *startButton_;
    Difficulty difficulty_ = Difficulty::Easy;
    QWidget *buildCard();
    void dismiss();
    void confirm();
};

}
