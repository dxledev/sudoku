/**
 * @file new_puzzle_modal.cpp
 * @brief Confirmation flow for replacing an unfinished puzzle.
 *
 * The in-window card traps keyboard focus, supports Escape cancellation, and
 * emits a difficulty only after explicit confirmation.
 */
#include "new_puzzle_modal.h"

#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <algorithm>

namespace sudoku {

NewPuzzleModal::NewPuzzleModal(QWidget *parent, QWidget *background) : ModalBackdrop(parent, background) {
    setObjectName("newPuzzleModal");
    setAccessibleName("Start a new puzzle dialog");
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet("QWidget#newPuzzleModal { background: transparent; }");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->addStretch();
    card_ = buildCard();
    connect(this, &ModalBackdrop::outsideClicked, this, &NewPuzzleModal::dismiss);
    layout->addWidget(card_, 0, Qt::AlignHCenter);
    layout->addStretch();
    hide();
}

QWidget *NewPuzzleModal::buildCard() {
    auto *card = new QFrame;
    card->setObjectName("newPuzzleCard");
    card->setMaximumWidth(460);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(20);
    auto *heading = new QLabel("Start a new puzzle?");
    heading->setObjectName("heading");
    heading->setAlignment(Qt::AlignCenter);
    layout->addWidget(heading);
    auto *description = new QLabel("Your current puzzle will be replaced.\nKeep playing, or make a fresh start.");
    description->setObjectName("muted");
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *actions = new QHBoxLayout;
    actions->addStretch();
    keepButton_ = new QPushButton("Keep playing");
    keepButton_->setObjectName("keepPlaying");
    keepButton_->setToolTip("Keep playing · Escape");
    keepButton_->setCursor(Qt::PointingHandCursor);
    connect(keepButton_, &QPushButton::clicked, this, &NewPuzzleModal::dismiss);
    actions->addWidget(keepButton_);
    startButton_ = new QPushButton("New puzzle");
    startButton_->setObjectName("startNewPuzzle");
    startButton_->setCursor(Qt::PointingHandCursor);
    connect(startButton_, &QPushButton::clicked, this, &NewPuzzleModal::confirm);
    actions->addWidget(startButton_);
    actions->addStretch();
    layout->addLayout(actions);
    return card;
}

void NewPuzzleModal::showConfirmation(Difficulty difficulty) {
    difficulty_ = difficulty;
    card_->setFixedWidth(std::min(460, std::max(0, width() - 48)));
    show();
    layout()->activate();
    raise();
    keepButton_->setFocus();
}

void NewPuzzleModal::setTheme(const Theme &theme) {
    setTint(theme.color("background"));
}

void NewPuzzleModal::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        event->accept();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (startButton_->hasFocus())
            confirm();
        else
            dismiss();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

bool NewPuzzleModal::focusNextPrevChild(bool) {
    (keepButton_->hasFocus() ? startButton_ : keepButton_)->setFocus();
    return true;
}

void NewPuzzleModal::resizeEvent(QResizeEvent *event) {
    ModalBackdrop::resizeEvent(event);
    card_->setFixedWidth(std::min(460, std::max(0, width() - 48)));
    layout()->activate();
}

void NewPuzzleModal::dismiss() {
    hide();
    emit dismissed();
}

void NewPuzzleModal::confirm() {
    hide();
    emit confirmed(difficulty_);
}

}
