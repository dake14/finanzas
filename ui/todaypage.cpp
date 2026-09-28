// ui/todaypage.cpp — lo que hay que hacer hoy.
//
// Desde el recorte, Hoy tiene dos cosas y ningun numero: el formulario de
// anotar y la lista de pendientes. Lo que se viene a hacer a esta pantalla es
// anotar y resolver; las cifras estan en Informes, ordenadas por la pregunta
// que contestan.

#include <QDate>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>
#include <QVBoxLayout>

#include "cards.hpp"
#include "entryform.hpp"
#include "pages.hpp"
#include "pendinglist.hpp"
#include "theme.hpp"

namespace dake::ui {

TodayPage::TodayPage(QWidget* parent) : QWidget(parent) {
    buildUi();
}

void TodayPage::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto* page = new QWidget(scroll);
    scroll->setWidget(page);

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 28);
    layout->setSpacing(16);

    heading_ = new QLabel(QStringLiteral("Hoy"), page);
    heading_->setFont(theme::displayFont(22, QFont::Bold));
    theme::setLabelColor(heading_, theme::kText);
    layout->addWidget(heading_);

    subheading_ = new QLabel(page);
    subheading_->setFont(theme::bodyFont(10));
    theme::setLabelColor(subheading_, theme::kTextMuted);
    layout->addWidget(subheading_);

    // --- Anotar, arriba de todo ---------------------------------------------
    entryCard_ = new Card(QStringLiteral("ANOTAR"), page);
    entry_ = new EntryForm(entryCard_);
    entryCard_->addContent(entry_);
    layout->addWidget(entryCard_);

    // --- Lo que pide accion ------------------------------------------------
    auto* pendingCard = new Card(QStringLiteral("PENDIENTES"), page);
    pending_ = new PendingList(pendingCard);
    pendingCard->addContent(pending_);
    layout->addWidget(pendingCard);

    layout->addStretch(1);
}

void TodayPage::setSnapshot(const Snapshot& snapshot) {
    const QDate today(snapshot.today.year, static_cast<int>(snapshot.today.month),
                      static_cast<int>(snapshot.today.day));
    subheading_->setText(QLocale(QLocale::Spanish).toString(today, QStringLiteral("dddd d 'de' MMMM")));

    entry_->setSnapshot(snapshot);
    pending_->setSnapshot(snapshot);

    const QString anywhere =
        snapshot.hotkeyRegistered
            ? QStringLiteral(" Desde cualquier programa: %1.")
                  .arg(QKeySequence(snapshot.hotkey, QKeySequence::PortableText)
                           .toString(QKeySequence::NativeText))
            : QString();
    entryCard_->setSubtitle(
        QStringLiteral("Monto, categoría, Enter. Alt+G gasto · Alt+I ingreso · Alt+T traspaso.") +
        anywhere);
}

} // namespace dake::ui
