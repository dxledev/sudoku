#pragma once

#include "core/puzzle.h"
#include "core/theme.h"
#include "modal_backdrop.h"

class QPushButton;

namespace sudoku {

class NewPuzzleModal : public ModalBackdrop {
    Q_OBJECT
public:
    explicit NewPuzzleModal(QWidget *parent, QWidget *background);
    void showConfirmation(Difficulty difficulty);
    void setTheme(const Theme &theme);

signals:
    void confirmed(Difficulty difficulty);
    void dismissed();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QPushButton *keepButton_, *startButton_;
    Difficulty difficulty_ = Difficulty::Easy;
    QWidget *buildCard();
    void dismiss();
    void confirm();
};

}
