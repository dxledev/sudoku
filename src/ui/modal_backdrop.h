#pragma once

#include <QPixmap>
#include <QWidget>

namespace sudoku {

class ModalBackdrop : public QWidget {
    Q_OBJECT
public:
    explicit ModalBackdrop(QWidget *parent, QWidget *background);
    void setTint(const QColor &color);

signals:
    void outsideClicked();

protected:
    QWidget *card_ = nullptr;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QWidget *background_;
    QPixmap snapshot_;
    QColor tint_;
    void capture(const QPixmap &snapshot);
    void clearSnapshot();
    void captureBackground();
};

}
