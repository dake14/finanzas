#pragma once
//
// ui/capturewindow.hpp — la ventana mini del atajo global.
//
// Sin marco, encima de todo, centrada en la pantalla donde esta el mouse. Se
// esconde al guardar, con Esc o al hacer clic en otro lado. Si se escondio sin
// guardar, lo escrito sigue ahi la proxima vez: cerrarla por error no puede
// costar lo que ya se habia escrito.
//
#include <QWidget>

namespace dake::ui {

class CaptureWidget;

class CaptureWindow : public QWidget {
    Q_OBJECT

public:
    explicit CaptureWindow(QWidget* parent = nullptr);

    [[nodiscard]] CaptureWidget* capture() const noexcept { return capture_; }

    /// La muestra al frente, con el foco en la linea y el cronometro andando.
    void popup();

protected:
    bool event(QEvent* event) override;

private:
    CaptureWidget* capture_ = nullptr;
};

} // namespace dake::ui
