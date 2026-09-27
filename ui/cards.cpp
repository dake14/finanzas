#include "cards.hpp"

#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include "theme.hpp"

namespace dake::ui {

Card::Card(const QString& title, QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("Card"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(4);

    if (!title.isEmpty()) {
        auto* titleLabel = new QLabel(title, this);
        titleLabel->setObjectName(QStringLiteral("CardTitle"));
        titleLabel->setFont(theme::bodyFont(10, QFont::DemiBold));
        theme::setLabelColor(titleLabel, theme::kAccent);
        layout->addWidget(titleLabel);
    }

    subtitle_ = new QLabel(this);
    subtitle_->setFont(theme::bodyFont(9));
    subtitle_->setWordWrap(true);
    subtitle_->setVisible(false);
    theme::setLabelColor(subtitle_, theme::kTextFaint);
    layout->addWidget(subtitle_);

    layout->addSpacing(8);

    body_ = layout;
}

void Card::addContent(QWidget* widget, int stretch) {
    // Solo el QWidget pelado que agrupa contenido: un campo o una tabla dentro
    // de la tarjeta conservan su fondo.
    if (widget->metaObject() == &QWidget::staticMetaObject) {
        widget->setProperty("cardBody", true);
    }
    body_->addWidget(widget, stretch);
}

void Card::setSubtitle(const QString& text) {
    subtitle_->setText(text);
    subtitle_->setVisible(!text.isEmpty());
}

KpiCard::KpiCard(const QString& title, theme::Tono accent, QWidget* parent)
    : QFrame(parent), accent_(accent) {
    setObjectName(QStringLiteral("Card"));
    setMinimumHeight(112);

    auto* layout = new QVBoxLayout(this);
    // El margen izquierdo deja aire para la franja de color que pinta
    // paintEvent; sin el, el texto se montaria encima.
    layout->setContentsMargins(24, 16, 18, 16);
    layout->setSpacing(2);

    title_ = new QLabel(title, this);
    title_->setFont(theme::bodyFont(9, QFont::DemiBold));
    theme::setLabelColor(title_, theme::kAccent);

    value_ = new QLabel(QStringLiteral("—"), this);
    value_->setFont(theme::figureFont(21));
    theme::setLabelColor(value_, theme::kText);

    note_ = new QLabel(this);
    note_->setFont(theme::bodyFont(9));
    theme::setLabelColor(note_, theme::kTextMuted);

    layout->addWidget(title_);
    layout->addStretch(1);
    layout->addWidget(value_);
    layout->addWidget(note_);
}

void KpiCard::setValue(const QString& value) {
    value_->setText(value);
}

void KpiCard::setNote(const QString& note, theme::Tono color) {
    note_->setText(note);
    theme::setLabelColor(note_, color);
}

void KpiCard::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Franja vertical de color pegada al borde izquierdo, con las esquinas
    // redondeadas del mismo radio que la tarjeta.
    QPainterPath path;
    path.addRoundedRect(QRectF(1.0, 14.0, 4.0, static_cast<double>(height()) - 28.0), 2.0, 2.0);
    painter.fillPath(path, theme::color(accent_));
}

} // namespace dake::ui
