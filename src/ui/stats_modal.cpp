#include "stats_modal.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QKeyEvent>
#include <QPainter>
#include <QResizeEvent>
#include <algorithm>

namespace sudoku {
namespace {

QString duration(qint64 seconds) {
    if (seconds >= 3600)
        return QString("%1:%2:%3").arg(seconds / 3600).arg(seconds / 60 % 60, 2, 10, QChar('0'))
            .arg(seconds % 60, 2, 10, QChar('0'));
    return QString("%1:%2").arg(seconds / 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
}

QLabel *text(const QString &value, const QString &name, QWidget *parent = nullptr) {
    auto *label = new QLabel(value, parent);
    label->setObjectName(name);
    label->setMinimumHeight(20);
    return label;
}

}

StatsModal::StatsModal(QWidget *parent) : QWidget(parent) {
    setObjectName("statsModal");
    setAccessibleName("Local statistics dialog");
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet("QWidget#statsModal { background: transparent; }");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->addStretch();
    card_ = buildCard();
    layout->addWidget(card_, 0, Qt::AlignHCenter);
    layout->addStretch();
    hide();
}

QWidget *StatsModal::buildCard() {
    auto *card = new QFrame;
    card->setObjectName("statsCard");
    card->setMaximumWidth(700);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(20);
    auto *header = new QHBoxLayout;
    header->addWidget(text("Your stats", "heading"));
    header->addStretch();
    auto *close = new QPushButton("Close");
    close->setObjectName("closeStats");
    close->setAccessibleName("Close statistics");
    close->setToolTip("Close · Escape");
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &StatsModal::dismiss);
    header->addWidget(close);
    layout->addLayout(header);
    auto *intro = text("A little progress, one puzzle at a time.", "muted");
    layout->addWidget(intro);
    buildTable(layout);
    auto *note = text("Times and average mistakes are for wins. Win rate is wins ÷ (wins + quits).\n"
                      "A quit is an unfinished puzzle replaced after at least 3 minutes of play and a final number entered. "
                      "Closing the app keeps your puzzle for later.", "tip");
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addWidget(text("Saved locally on this device · Esc to close", "shortcut"));
    return card;
}

void StatsModal::buildTable(QVBoxLayout *layout) {
    auto *table = new QGridLayout;
    table->setHorizontalSpacing(14);
    table->setVerticalSpacing(18);
    const QStringList headers{"Difficulty", "Wins", "Avg time", "Best time", "Avg mistakes", "Quits", "Win rate"};
    for (int column = 0; column < headers.size(); ++column) {
        auto *heading = text(headers[column], "statsHeading");
        heading->setAlignment(column ? Qt::AlignRight : Qt::AlignLeft);
        table->addWidget(heading, 0, column);
    }
    for (int row = 0; row < 4; ++row) {
        auto name = QString::fromUtf8(difficulties[row].name.data());
        name[0] = name[0].toUpper();
        table->addWidget(text(name, "statsDifficulty"), row + 1, 0);
        for (int column = 0; column < 6; ++column) {
            auto *value = text("—", QString("stats_%1_%2").arg(row).arg(column));
            value->setAlignment(Qt::AlignRight);
            value->setAccessibleName(name + " " + headers[column + 1]);
            values_[row][column] = value;
            table->addWidget(value, row + 1, column + 1);
        }
    }
    layout->addLayout(table);
}

void StatsModal::showStats(const Statistics &statistics) {
    for (int row = 0; row < 4; ++row) {
        const auto &stats = statistics.at(difficulties[row].value);
        auto &values = values_[row];
        values[0]->setText(QString::number(stats.wins));
        values[1]->setText(stats.wins ? duration(qRound64(double(stats.totalSeconds) / stats.wins)) : "—");
        values[2]->setText(stats.wins ? duration(stats.bestSeconds) : "—");
        values[3]->setText(stats.wins ? QString::number(double(stats.totalMistakes) / stats.wins, 'f', 1) : "—");
        values[4]->setText(QString::number(stats.quits));
        values[5]->setText(stats.wins + stats.quits ? QString::number(100.0 * stats.wins / (stats.wins + stats.quits), 'f', 0) + "%" : "—");
    }
    card_->setFixedWidth(std::min(700, std::max(0, width() - 48)));
    show();
    layout()->activate();
    raise();
    findChild<QPushButton *>("closeStats")->setFocus();
}

void StatsModal::setTheme(const Theme &theme) {
    backdrop_ = theme.color("background");
    backdrop_.setAlpha(210);
    update();
}

void StatsModal::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), backdrop_);
}

void StatsModal::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        dismiss();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void StatsModal::dismiss() {
    hide();
    emit dismissed();
}

bool StatsModal::focusNextPrevChild(bool) {
    findChild<QPushButton *>("closeStats")->setFocus();
    return true;
}

void StatsModal::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    card_->setFixedWidth(std::min(700, std::max(0, width() - 48)));
    layout()->activate();
}

}
