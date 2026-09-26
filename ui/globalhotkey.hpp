#pragma once
//
// ui/globalhotkey.hpp — un atajo de teclado que funciona desde cualquier
// programa de Windows, no solo con la ventana de Finanzas al frente.
//
// Es lo que hace que anotar un gasto no exija abrir la aplicacion: Ctrl+Alt+
// Espacio desde el navegador, el editor o donde sea, y aparece la ventana mini.
// Usa RegisterHotKey, que es la forma que Windows ofrece para esto; en otro
// sistema setShortcut devuelve false y la aplicacion sigue funcionando sin
// atajo global.
//
#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>

namespace dake::ui {

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;

    /// Registra el atajo, soltando el anterior. false si Windows no lo acepta
    /// —casi siempre porque otro programa ya lo tiene— o si la tecla no es una
    /// que se pueda registrar (letras, numeros, espacio, F1-F12).
    bool setShortcut(const QKeySequence& sequence);

    [[nodiscard]] QKeySequence shortcut() const { return sequence_; }
    [[nodiscard]] bool isRegistered() const noexcept { return registered_; }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void activated();

private:
    void release();

    QKeySequence sequence_;
    bool registered_ = false;
};

} // namespace dake::ui
