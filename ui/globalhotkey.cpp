#include "globalhotkey.hpp"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace dake::ui {
namespace {

/// Identificador del atajo dentro de este proceso. Cualquier numero entre
/// 0x0000 y 0xBFFF sirve; lo que importa es usar siempre el mismo.
constexpr int kHotkeyId = 0x0DA1;

#ifdef Q_OS_WIN
/// La tecla virtual de Windows para una tecla de Qt, o 0 si no se soporta.
[[nodiscard]] UINT virtualKey(Qt::Key key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return static_cast<UINT>('A' + (key - Qt::Key_A));
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return static_cast<UINT>('0' + (key - Qt::Key_0));
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        return static_cast<UINT>(VK_F1 + (key - Qt::Key_F1));
    }
    if (key == Qt::Key_Space) {
        return VK_SPACE;
    }
    return 0;
}
#endif

} // namespace

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent) {
    QCoreApplication::instance()->installNativeEventFilter(this);
}

GlobalHotkey::~GlobalHotkey() {
    release();
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

bool GlobalHotkey::setShortcut(const QKeySequence& sequence) {
    release();
    sequence_ = sequence;
#ifdef Q_OS_WIN
    if (sequence.isEmpty()) {
        return false;
    }
    const QKeyCombination combination = sequence[0];
    const UINT vk = virtualKey(combination.key());
    if (vk == 0) {
        return false;
    }
    UINT modifiers = MOD_NOREPEAT;
    const Qt::KeyboardModifiers mods = combination.keyboardModifiers();
    if (mods.testFlag(Qt::ControlModifier)) modifiers |= MOD_CONTROL;
    if (mods.testFlag(Qt::AltModifier)) modifiers |= MOD_ALT;
    if (mods.testFlag(Qt::ShiftModifier)) modifiers |= MOD_SHIFT;
    if (mods.testFlag(Qt::MetaModifier)) modifiers |= MOD_WIN;

    // Sin ventana: WM_HOTKEY llega a la cola del hilo, y el despachador de
    // eventos de Qt se lo pasa a nativeEventFilter como cualquier otro mensaje.
    registered_ = RegisterHotKey(nullptr, kHotkeyId, modifiers, vk) != 0;
    return registered_;
#else
    return false;
#endif
}

void GlobalHotkey::release() {
#ifdef Q_OS_WIN
    if (registered_) {
        UnregisterHotKey(nullptr, kHotkeyId);
    }
#endif
    registered_ = false;
}

bool GlobalHotkey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(result);
#ifdef Q_OS_WIN
    if (registered_ && eventType == "windows_generic_MSG") {
        const auto* msg = static_cast<const MSG*>(message);
        if (msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == kHotkeyId) {
            emit activated();
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

} // namespace dake::ui
