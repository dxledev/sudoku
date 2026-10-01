#pragma once

#include "core/statistics.h"
#include "core/theme.h"
#include "modal_backdrop.h"
#include <QLabel>
#include <QVBoxLayout>

namespace sudoku {

/** @brief Embedded statistics table that pauses play while visible. */
class StatsModal : public ModalBackdrop {
    Q_OBJECT
public:
    /** @brief Create the statistics layer and its table card. */
    explicit StatsModal(QWidget *parent, QWidget *background);
    /** @brief Populate and display values from the supplied statistics snapshot. */
    void showStats(const Statistics &statistics);
    /** @brief Apply a new theme to the card and its text. */
    void setTheme(const Theme &theme);

signals:
    /** @brief Emitted when the statistics modal is dismissed. */
    void dismissed();

protected:
    /** @brief Dismiss the modal on Escape and handle focused-button activation. */
    void keyPressEvent(QKeyEvent *event) override;
    /** @brief Keep keyboard focus traversal within the modal card. */
    bool focusNextPrevChild(bool next) override;
    /** @brief Re-center the card after the parent window changes size. */
    void resizeEvent(QResizeEvent *event) override;

private:
    std::array<std::array<QLabel *, 6>, 4> values_{};
    /** @brief Close the modal and notify the owning window. */
    void dismiss();
    /** @brief Construct the centered statistics card. */
    QWidget *buildCard();
    /** @brief Add difficulty rows and metric headings to the card layout. */
    void buildTable(QVBoxLayout *layout);
};

}
