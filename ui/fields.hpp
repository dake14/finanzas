#pragma once
//
// ui/fields.hpp — leer y escribir montos y horas en campos de texto.
//
// Los montos se escriben como se dicen ("25,50", "25.50", "1.200"); las horas,
// en horas con coma ("2,5"), y se guardan en minutos. Nada de esto calcula:
// solo traduce entre lo que se escribe y lo que guarda el nucleo.
//
#include <QString>

#include <cmath>
#include <optional>
#include <stdexcept>

#include "dake/core/capture.hpp"
#include "dake/core/money.hpp"
#include "theme.hpp"

namespace dake::ui {

/// Centavos, o vacio si el texto no es un monto. Vacio tambien si esta vacio.
[[nodiscard]] inline std::optional<std::int64_t> parseMoneyText(const QString& text,
                                                                core::Currency currency) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }
    // El interprete de la captura ya sabe leer "1.200" como mil doscientos y
    // "25,50" como veinticinco cincuenta: se usa el mismo para no tener dos
    // reglas distintas segun donde se escriba un monto.
    core::CaptureContext context;
    context.currency = currency;
    const auto draft = core::parseCapture(trimmed.toStdString(), context);
    if (!draft.amountMinor || !draft.description.empty()) {
        return std::nullopt;
    }
    return draft.amountMinor;
}

/// Minutos, o vacio. "2,5" y "2.5" son dos horas y media.
[[nodiscard]] inline std::optional<int> parseHoursText(const QString& text) {
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }
    trimmed.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double hours = trimmed.toDouble(&ok);
    if (!ok || hours < 0 || hours > 1000) {
        return std::nullopt;
    }
    return static_cast<int>(std::lround(hours * 60.0));
}

/// "2,5" (sin la h: es lo que se pone en un campo).
[[nodiscard]] inline QString hoursText(int minutes) {
    const double hours = static_cast<double>(minutes) / 60.0;
    QString text = QString::number(hours, 'f', std::fmod(hours, 1.0) == 0.0 ? 0 : 2);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0'))) text.chop(1);
        if (text.endsWith(QLatin1Char('.'))) text.chop(1);
    }
    return text.replace(QLatin1Char('.'), QLatin1Char(','));
}

/// Un monto para un campo editable: "25,50", sin separador de miles.
[[nodiscard]] inline QString moneyFieldText(std::int64_t minor, core::Currency currency) {
    if (minor == 0) {
        return QString();
    }
    return theme::formatMoney(core::Money::fromMinor(minor, currency)).remove(QLatin1Char('.'));
}

} // namespace dake::ui
