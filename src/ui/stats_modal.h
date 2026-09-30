#pragma once

#include "core/statistics.h"
#include "core/theme.h"
#include "modal_backdrop.h"
#include <QLabel>
#include <QVBoxLayout>

namespace sudoku {

class StatsModal : public ModalBackdrop {
    Q_OBJECT
public:
    explicit StatsModal(QWidget *parent, QWidget *background);
    void showStats(const Statistics &statistics);
    void setTheme(const Theme &theme);

signals:
    void dismissed();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    std::array<std::array<QLabel *, 6>, 4> values_{};
    void dismiss();
    QWidget *buildCard();
    void buildTable(QVBoxLayout *layout);
};

}
