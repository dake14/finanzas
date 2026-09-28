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

class EntryForm;

class CaptureWindow : public QWidget {
    Q_OBJECT

public:
    explicit CaptureWindow(QWidget* parent = nullptr);

    [[nodiscard]] EntryForm* entry() const noexcept { return entry_; }

    /// La muestra al frente, con el foco en el monto.
    void popup();

protected:
    bool event(QEvent* event) override;

private:
    EntryForm* entry_ = nullptr;
};

} // namespace dake::ui
