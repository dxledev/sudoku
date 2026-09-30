#pragma once

#include "core/statistics.h"
#include "core/theme.h"
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

namespace sudoku {

class StatsModal : public QWidget {
    Q_OBJECT
public:
    explicit StatsModal(QWidget *parent);
    void showStats(const Statistics &statistics);
    void setTheme(const Theme &theme);

signals:
    void dismissed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QColor backdrop_;
    QWidget *card_;
    std::array<std::array<QLabel *, 6>, 4> values_{};
    void dismiss();
    QWidget *buildCard();
    void buildTable(QVBoxLayout *layout);
};

}
