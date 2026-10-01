#pragma once

#include "core/puzzle.h"
#include "core/theme.h"
#include "modal_backdrop.h"

class QPushButton;

namespace sudoku {

/** @brief Embedded confirmation dialog for replacing the current puzzle. */
class NewPuzzleModal : public ModalBackdrop {
    Q_OBJECT
public:
    /** @brief Create the confirmation layer and its keyboard-accessible card. */
    explicit NewPuzzleModal(QWidget *parent, QWidget *background);
    /** @brief Show the prompt for starting a puzzle at the requested tier. */
    void showConfirmation(Difficulty difficulty);
    /** @brief Apply a new palette while the modal is open or hidden. */
    void setTheme(const Theme &theme);

signals:
    /** @brief Emitted after the user confirms the requested difficulty. */
    void confirmed(Difficulty difficulty);
    /** @brief Emitted when the modal closes without starting a puzzle. */
    void dismissed();

protected:
    /** @brief Dismiss on Escape and route confirmation keys to the focused button. */
    void keyPressEvent(QKeyEvent *event) override;
    /** @brief Keep Tab navigation confined to the modal's two actions. */
    bool focusNextPrevChild(bool next) override;
    /** @brief Re-center the card when the parent window changes size. */
    void resizeEvent(QResizeEvent *event) override;

private:
    QPushButton *keepButton_, *startButton_;
    Difficulty difficulty_ = Difficulty::Easy;
    /** @brief Construct the centered confirmation card and controls. */
    QWidget *buildCard();
    /** @brief Close the prompt and notify the window that it was dismissed. */
    void dismiss();
    /** @brief Close the prompt and emit the confirmed difficulty. */
    void confirm();
};

}
