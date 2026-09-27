#include "capturewindow.hpp"

#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>

#include "capturewidget.hpp"
#include "theme.hpp"

namespace dake::ui {

CaptureWindow::CaptureWindow(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
    setWindowTitle(QStringLiteral("Anotar"));
    setAttribute(Qt::WA_TranslucentBackground);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("CaptureFrame"));
    outer->addWidget(frame);

    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 14, 20, 12);
    layout->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("ANOTAR"), frame);
    title->setFont(theme::bodyFont(8, QFont::DemiBold));
    theme::setLabelColor(title, theme::kAccent);
    layout->addWidget(title);

    capture_ = new CaptureWidget(frame);
    layout->addWidget(capture_);

    setFixedWidth(960);
}

void CaptureWindow::popup() {
    // En la pantalla donde esta el mouse, un tercio hacia abajo: donde ya se
    // esta mirando, no en el monitor de al lado.
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    adjustSize();
    const QRect area = screen->availableGeometry();
    move(area.center().x() - width() / 2, area.top() + area.height() / 3 - height() / 2);

    show();
    raise();
    activateWindow();
    capture_->startClock();
    capture_->focusInput();
}

bool CaptureWindow::event(QEvent* event) {
    if (event->type() == QEvent::WindowDeactivate) {
        hide();
    }
    return QWidget::event(event);
}

} // namespace dake::ui
