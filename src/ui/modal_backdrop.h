#pragma once

#include <QPixmap>
#include <QWidget>
#include <functional>

namespace sudoku {

class ModalMotion;

/** @brief In-window modal surface with a blurred snapshot and outside-click signal. */
class ModalBackdrop : public QWidget {
    Q_OBJECT
public:
    /** @brief Create a modal layer over @p background inside @p parent. */
    explicit ModalBackdrop(QWidget *parent, QWidget *background);
    /** @brief Set the translucent tint drawn above the captured background. */
    void setTint(const QColor &color);

signals:
    /** @brief Emitted when a click lands outside the modal card. */
    void outsideClicked();

protected:
    QWidget *card_ = nullptr;
    /** @brief Show the layer and animate the card and backdrop in without a visible flash. */
    void present();
    /** @brief Animate out, then hide and run @p after. Ignored while already closing. */
    void closeAnimated(std::function<void()> after);
    /** @brief True while the closing transition is running. */
    bool isClosing() const { return closing_; }
    /** @brief Draw the blurred snapshot and tint behind the modal card. */
    void paintEvent(QPaintEvent *event) override;
    /** @brief Capture the background and size the card when shown. */
    void showEvent(QShowEvent *event) override;
    /** @brief Release the captured background when hidden. */
    void hideEvent(QHideEvent *event) override;
    /** @brief Keep the card centered after a parent resize. */
    void resizeEvent(QResizeEvent *event) override;
    /** @brief Emit outsideClicked when the press does not land on the card. */
    void mousePressEvent(QMouseEvent *event) override;

private:
    QWidget *background_;
    QPixmap snapshot_;
    QColor tint_;
    ModalMotion *motion_ = nullptr;
    std::function<void()> afterClose_;
    bool closing_ = false;
    /** @brief Store and blur a newly captured background image. */
    void capture(const QPixmap &snapshot);
    /** @brief Release the image to avoid retaining a hidden modal snapshot. */
    void clearSnapshot();
    /** @brief Capture the background widget at the modal's display resolution. */
    void captureBackground();
};

}
