#include "appbridge.hpp"

#include "bitacora.hpp"

#include <QDate>
#include <QDateTime>
#include <QTimer>

#include <algorithm>
#include <stdexcept>

#include "dake/core/format.hpp"
#include "dake/core/report.hpp"
#include "dake/storage/exchange.hpp"
#include "dake/sync/config.hpp"

namespace dake::mobile {
namespace {

/// Cuanto se espera despues del ultimo cambio antes de subir. El mismo valor
/// que en el escritorio, a proposito: si las dos aplicaciones esperaran
/// distinto, la que espera menos ganaria siempre los conflictos y eso no
/// tendria nada que ver con cual dato es el bueno.
constexpr int kAutoSyncDelayMs = 5000;

[[nodiscard]] QString qs(const std::string& text) {
    return QString::fromStdString(text);
}

[[nodiscard]] QString kindLabel(core::PocketKind kind) {
    switch (kind) {
        case core::PocketKind::Operacion: return QStringLiteral("Operacion");
        case core::PocketKind::Ahorro: return QStringLiteral("Ahorro");
        case core::PocketKind::Inversion: return QStringLiteral("Inversion");
        case core::PocketKind::Personal: return QStringLiteral("Personal");
        case core::PocketKind::Emergencia: return QStringLiteral("Emergencia");
    }
    return {};
}

[[nodiscard]] QString movementKindLabel(core::MovementKind kind) {
    switch (kind) {
        case core::MovementKind::Ingreso: return QStringLiteral("Ingreso");
        case core::MovementKind::Gasto: return QStringLiteral("Gasto");
        case core::MovementKind::Traspaso: return QStringLiteral("Traspaso");
    }
    return {};
}

/// El numero que manda QML es el mismo que el del enum del nucleo. Tener dos
/// numeraciones —una para la pantalla y otra para guardar— es como se termina
/// anotando un ingreso donde iba un gasto sin que nada falle ruidosamente.
[[nodiscard]] core::MovementKind kindFromIndex(int index) {
    switch (index) {
        case 0: return core::MovementKind::Ingreso;
        case 2: return core::MovementKind::Traspaso;
        default: return core::MovementKind::Gasto;
    }
}

/// En Android el selector del sistema devuelve una URL `content://`, no una
/// ruta. QFile las entiende tal cual; convertirlas con toLocalFile() devuelve
/// vacio y el archivo no se abre nunca.
[[nodiscard]] QString pathFromUrl(const QUrl& url) {
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

} // namespace

AppBridge::AppBridge(QObject* parent)
    : QObject(parent), currency_(core::Currency::usd()), today_(core::Date::fromYmd(2026, 1, 1)) {
    // El constructor NO lanza, y eso es una decision de producto, no de estilo.
    //
    // En Android una excepcion que sale de main() mata el proceso antes de que
    // exista una sola ventana, y lo que ve el usuario es una pantalla negra sin
    // ningun mensaje. Un fallo al abrir la base —permisos, disco lleno, una
    // migracion que no puede correr— tiene que terminar en una pantalla que
    // explique que paso, no en la aplicacion desapareciendo.
    try {
        fatalError_ = abrir();
    } catch (const std::exception& error) {
        fatalError_ = QString::fromUtf8(error.what());
    } catch (...) {
        fatalError_ = QStringLiteral("Fallo desconocido al abrir la base.");
    }

    if (!fatalError_.isEmpty()) {
        bitacora::anotar(QStringLiteral("FALLO el arranque: ") + fatalError_);
        // Lo que se haya alcanzado a abrir se cierra: dejar medio motor armado
        // hace que la primera pantalla que lo toque falle de una forma
        // distinta y mas dificil de leer que la que ya tenemos escrita.
        syncEngine_.reset();
        supabase_.reset();
        repository_.reset();
        db_.reset();
    } else {
        bitacora::anotar(QStringLiteral("Arranque listo."));
    }
}

QString AppBridge::abrir() {
    bitacora::anotar(QStringLiteral("Abriendo la base."));
    db_ = std::make_unique<storage::Database>(storage::Database::defaultPath());
    repository_ = std::make_unique<storage::Repository>(*db_);
    deviceId_ = storage::deviceId(*repository_);
    clock_ = std::make_unique<core::HlcClock>(deviceId_.toStdString());

    supabase_ = std::make_unique<sync::SupabaseClient>(sync::SupabaseConfig::load(), this);
    syncEngine_ = std::make_unique<sync::SyncEngine>(*supabase_, *repository_, this);

    connect(supabase_.get(), &sync::SupabaseClient::signedIn, this, [this](const QString& email) {
        repository_->setSetting(QStringLiteral("sync.refresh_token"), supabase_->refreshToken());
        repository_->setSetting(QStringLiteral("sync.user_email"), email);
        cloudStatus_ = QStringLiteral("Conectado.");
        emit cloudChanged();
        syncEngine_->sync();
    });
    connect(supabase_.get(), &sync::SupabaseClient::signedOut, this, [this]() {
        cloudStatus_ = QStringLiteral("Sin conectar.");
        emit cloudChanged();
    });
    connect(supabase_.get(), &sync::SupabaseClient::authFailed, this, [this](const QString& message) {
        cloudStatus_ = message;
        emit cloudChanged();
    });

    connect(syncEngine_.get(), &sync::SyncEngine::progress, this, [this](const QString& text, int, int) {
        cloudStatus_ = text;
        emit cloudChanged();
    });
    connect(syncEngine_.get(), &sync::SyncEngine::finished, this, [this](int uploaded, int downloaded, const QString& error) {
        if (error.isEmpty()) {
            cloudStatus_ = QStringLiteral("%1 ↑, %2 ↓").arg(uploaded).arg(downloaded);
        } else {
            cloudStatus_ = error;
        }
        reload();
        emit dataChanged();
        emit cloudChanged();
    });

    autoSyncTimer_ = new QTimer(this);
    autoSyncTimer_->setSingleShot(true);
    autoSyncTimer_->setInterval(kAutoSyncDelayMs);
    connect(autoSyncTimer_, &QTimer::timeout, this, &AppBridge::runAutoSync);

    const auto token = repository_->setting(QStringLiteral("sync.refresh_token"));
    if (token) {
        bitacora::anotar(QStringLiteral("Reanudando la sesion guardada."));
        supabase_->restoreSession(*token);
    }

    const QDate now = QDate::currentDate();
    today_ = core::Date::fromYmd(now.year(), static_cast<unsigned>(now.month()),
                                 static_cast<unsigned>(now.day()));

    // Igual que en la version de escritorio: arrancar en blanco no deja nada
    // contra que comparar. Solo corre sobre una base sin un solo movimiento:
    // sobre datos existentes no hace nada, asi que no puede pisar lo anotado.
    bitacora::anotar(QStringLiteral("Sembrando si hace falta."));
    repository_->seedIfEmpty(currency_);

    bitacora::anotar(QStringLiteral("Calculando las pantallas."));
    reload();
    return {};
}

AppBridge::~AppBridge() = default;

QString AppBridge::today() const {
    return qs(today_.toIso());
}

QString AppBridge::dbPath() const {
    return db_ ? db_->path() : storage::Database::defaultPath();
}

QStringList AppBridge::startupLog() const {
    return bitacora::ultimas(60);
}

QString AppBridge::startupLogPath() const {
    return bitacora::ruta();
}

void AppBridge::stamp(std::string& hlc, std::string& deviceId) {
    hlc = clock_->now(QDateTime::currentMSecsSinceEpoch()).encode();
    deviceId = deviceId_.toStdString();
}

// -------------------------------------------------------------- Recarga ----

void AppBridge::reload() {
    if (!repository_) {
        return;
    }
    // Se lee UNA vez y se guarda. Antes cada pregunta —las categorias ya
    // usadas, borrar un movimiento, cuadrar un bolsillo, el cierre de un mes—
    // volvia a leer la base entera. Son cientos de filas, no millones, pero en
    // un telefono cada lectura de mas es bateria y no hay ninguna razon para
    // pagarla.
    pocketsData_ = repository_->loadPockets();
    jobsData_ = repository_->loadJobs();
    movementsData_ = repository_->loadMovements();

    const auto& pockets = pocketsData_;
    const auto& jobs = jobsData_;
    const auto& movements = movementsData_;

    const auto balances = core::pocketBalances(pockets, movements, currency_, today_);

    // --- Bolsillos ---
    pockets_.clear();
    for (const core::PocketBalance& balance : balances) {
        QVariantMap row;
        row["id"] = qs(balance.pocketId);
        row["name"] = qs(balance.name);
        row["kind"] = static_cast<int>(balance.kind);
        row["kindLabel"] = kindLabel(balance.kind);
        row["balance"] = qs(core::formatAmount(balance.balance));
        row["balanceMinor"] = static_cast<qlonglong>(balance.balance.minor());
        row["negative"] = balance.balance.isNegative();
        row["pending"] = balance.pendingIn.isZero()
                             ? QString()
                             : qs(core::formatAmount(balance.pendingIn));
        pockets_.append(row);
    }

    // --- Trabajos: solo los abiertos, que son a los que se imputa ---
    jobs_.clear();
    for (const core::JobResult& result : core::jobResults(jobs, movements, currency_)) {
        if (result.closed) {
            continue;
        }
        QVariantMap row;
        row["id"] = qs(result.jobId);
        row["name"] = qs(result.name);
        row["client"] = qs(result.client);
        row["label"] = result.client.empty()
                           ? qs(result.name)
                           : qs(result.name) + QStringLiteral(" · ") + qs(result.client);
        row["margin"] = qs(core::formatAmount(result.margin));
        row["marginBps"] = result.income.isZero() ? QString()
                                                  : qs(core::formatBps(result.marginBps));
        jobs_.append(row);
    }

    // --- Ultimos movimientos ---
    recent_.clear();
    for (const core::Movement& movement : core::recentMovements(movements, 60)) {
        QVariantMap row;
        row["id"] = qs(movement.id);
        row["date"] = qs(movement.date.toIso());
        row["name"] = qs(movement.name);
        row["kind"] = static_cast<int>(movement.kind);
        row["kindLabel"] = movementKindLabel(movement.kind);
        row["category"] = qs(movement.category);
        row["settled"] = movement.settled;
        row["spreadMonths"] = movement.spreadMonths;

        QString where;
        for (const core::Pocket& pocket : pockets) {
            if (pocket.id == movement.pocketId) {
                where = qs(pocket.name);
            }
        }
        if (movement.kind == core::MovementKind::Traspaso) {
            for (const core::Pocket& pocket : pockets) {
                if (pocket.id == movement.targetPocketId) {
                    where += QStringLiteral(" → ") + qs(pocket.name);
                }
            }
        }
        row["where"] = where;

        QString jobName;
        for (const core::Job& job : jobs) {
            if (job.id == movement.jobId) {
                jobName = qs(job.name);
            }
        }
        row["job"] = jobName;

        const core::Money amount = core::Money::fromMinor(movement.amountMinor, currency_);
        const QString sign = movement.kind == core::MovementKind::Ingreso
                                 ? QStringLiteral("+")
                                 : (movement.kind == core::MovementKind::Gasto
                                        ? QStringLiteral("−")
                                        : QString());
        row["amount"] = sign + qs(core::formatAmount(amount));
        recent_.append(row);
    }

    // --- El resumen del mes ---
    const core::Date from = today_.firstDayOfMonth();
    const core::Date to = today_.lastDayOfMonth();
    const core::CashFlow flow = core::cashFlow(movements, currency_, from, to);
    const core::Funding fund = core::funding(pockets, movements, currency_, from, to);

    core::Money pending = core::Money::zero(currency_);
    for (const core::PocketBalance& balance : balances) {
        pending += balance.pendingIn;
    }

    summary_.clear();
    summary_["cash"] =
        qs(core::formatAmount(core::totalFor(balances, core::PocketKind::Operacion, currency_)));
    summary_["cashNegative"] =
        core::totalFor(balances, core::PocketKind::Operacion, currency_).isNegative();
    summary_["reserves"] = qs(core::formatAmount(fund.reserveBalance));
    summary_["pending"] = qs(core::formatAmount(pending));
    summary_["cashDelta"] = qs(core::formatAmount(flow.cashDelta));
    summary_["cashDeltaNegative"] = flow.cashDelta.isNegative();
    summary_["result"] = qs(core::formatAmount(flow.result));
    summary_["resultNegative"] = flow.result.isNegative();
    summary_["eating"] = fund.eatingReserves();
    summary_["fromReserves"] = qs(core::formatAmount(fund.net));

    if (fund.eatingReserves()) {
        summary_["headline"] =
            QStringLiteral("%1 de lo que gastaste este mes salio de tus ahorros.")
                .arg(qs(core::formatAmount(fund.net)));
        const int runway = fund.monthsOfRunway(1);
        summary_["headlineDetail"] =
            runway >= 0 ? QStringLiteral("A este ritmo te quedan cerca de %1 meses de reserva.")
                              .arg(runway)
                        : QStringLiteral("Eso es capital tuyo tapando un hueco, no una perdida.");
    } else if (!fund.toReserves.isZero()) {
        summary_["headline"] = QStringLiteral("Este mes guardaste %1.")
                                   .arg(qs(core::formatAmount(fund.toReserves - fund.fromReserves)));
        summary_["headlineDetail"] =
            QStringLiteral("El mes se pago solo y ademas sobro.");
    } else {
        summary_["headline"] = QStringLiteral("Este mes se pago con lo que cobraste.");
        summary_["headlineDetail"] = QStringLiteral("Ni entro ni salio plata de las reservas.");
    }

    // --- Graficas e indicadores -------------------------------------------
    months_.clear();
    const std::vector<core::MonthSummary> months = core::summarizeByMonth(pockets, movements, currency_);
    core::Money accumulatedCash = core::Money::zero(currency_);
    for (const core::MonthSummary& ms : months) {
        accumulatedCash += (ms.incomeAccrued - ms.cost);
        QVariantMap row;
        row["etiqueta"] = qs(ms.label());
        row["resultado"] = static_cast<double>(ms.result.minor()) / 100.0;
        row["ingresos"] = static_cast<double>(ms.incomeAccrued.minor()) / 100.0;
        row["costos"] = static_cast<double>(ms.cost.minor()) / 100.0;
        row["caja"] = static_cast<double>(accumulatedCash.minor()) / 100.0;
        row["resultadoTexto"] = qs(core::formatAmount(ms.result));
        row["cajaTexto"] = qs(core::formatAmount(accumulatedCash));
        row["resultadoNegativo"] = ms.result.isNegative();

        // De donde salio la plata ese mes. Es la frase que la pantalla de
        // Bolsillos pone al lado de cada mes, y la misma que arma el
        // escritorio: sacar de la reserva no es una perdida y no aparece en
        // ningun resultado, pero se repite hasta que no queda reserva.
        if (ms.netFunding.isNegative()) {
            row["fondeoTexto"] = QStringLiteral("y guardo %1")
                                     .arg(qs(core::formatAmount(core::Money::zero(currency_) - ms.netFunding)));
            row["fondeoNivel"] = 1;   // guardo
        } else if (ms.netFunding.isZero()) {
            row["fondeoTexto"] = QStringLiteral("sin tocar las reservas");
            row["fondeoNivel"] = 0;   // neutro
        } else {
            row["fondeoTexto"] = QStringLiteral("y saco %1 de las reservas")
                                     .arg(qs(core::formatAmount(ms.netFunding)));
            row["fondeoNivel"] = 2;   // saco
        }
        months_.append(row);
    }

    categories_.clear();
    const std::vector<core::CategoryTotal> cats = core::costByCategory(movements, currency_, from, to);
    for (const core::CategoryTotal& ct : cats) {
        QVariantMap row;
        row["etiqueta"] = qs(ct.category);
        row["valor"] = static_cast<double>(ct.total.minor()) / 100.0;
        row["texto"] = qs(core::formatAmount(ct.total));
        categories_.append(row);
    }

    // --- La pantalla de Trabajos entera ---
    //
    // Los mismos numeros y en el mismo orden que la tabla del escritorio. Si
    // aca se calculara algo distinto, las dos aplicaciones diriaan cosas
    // distintas del mismo trabajo y no habria forma de saber cual miente.
    allJobs_.clear();
    core::Money totalMargin = core::Money::zero(currency_);
    core::Money totalJobPending = core::Money::zero(currency_);

    jobMargins_.clear();
    const std::vector<core::JobResult> jrs = core::jobResults(jobs, movements, currency_);
    for (const core::JobResult& jr : jrs) {
        totalMargin += jr.margin;
        totalJobPending += jr.pending;

        QVariantMap fila;
        fila["id"] = qs(jr.jobId);
        fila["name"] = qs(jr.name);
        fila["client"] = qs(jr.client);
        fila["closed"] = jr.closed;
        fila["income"] = qs(core::formatAmount(jr.income));
        fila["cost"] = qs(core::formatAmount(jr.cost));
        fila["margin"] = qs(core::formatAmount(jr.margin));
        fila["marginNegative"] = jr.margin.isNegative();
        fila["marginBps"] = jr.income.isZero() ? QStringLiteral("—")
                                               : qs(core::formatBps(jr.marginBps));
        fila["pending"] = jr.pending.isZero() ? QString()
                                              : qs(core::formatAmount(jr.pending));
        // El estado es una sola linea porque en un telefono no entra una tabla
        // de siete columnas. Lo que mas importa saber de un trabajo no es si
        // esta abierto: es si te deben plata.
        fila["state"] = jr.pending.isZero()
                            ? (jr.closed ? QStringLiteral("cerrado") : QStringLiteral("abierto"))
                            : QStringLiteral("sin cobrar %1").arg(qs(core::formatAmount(jr.pending)));
        fila["stateLevel"] = jr.pending.isZero() ? 0 : 1;
        allJobs_.append(fila);

        QVariantMap row;
        row["etiqueta"] = qs(jr.name);
        row["valor"] = static_cast<double>(jr.margin.minor()) / 100.0;
        row["texto"] = QStringLiteral("%1 (%2)").arg(qs(core::formatAmount(jr.margin)), qs(core::formatBps(jr.marginBps)));
        jobMargins_.append(row);
    }

    jobsSummary_.clear();
    jobsSummary_["margin"] = qs(core::formatAmount(totalMargin));
    jobsSummary_["structure"] = qs(core::formatAmount(core::overhead(movements, currency_, from, to)));
    jobsSummary_["pending"] = totalJobPending.isZero() ? QString()
                                                       : qs(core::formatAmount(totalJobPending));
    jobsSummary_["count"] = static_cast<int>(jrs.size());

    // --- Lo que va arriba de Bolsillos ---
    const core::Money todoElDinero = core::totalAll(balances, currency_);
    const core::Money reserva = core::reserveTotal(balances, currency_);
    pocketsSummary_.clear();
    pocketsSummary_["total"] = qs(core::formatAmount(todoElDinero));
    pocketsSummary_["reserves"] = qs(core::formatAmount(reserva));
    pocketsSummary_["negative"] = todoElDinero.isNegative();

    stats_.clear();
    const core::BreakEven be = core::breakEven(jobs, movements, currency_, from, to);
    if (be.unknown()) {
        stats_["equilibrio"] = QStringLiteral("—");
        stats_["equilibrioNota"] = QStringLiteral("hacen falta trabajos con margen para saberlo");
    } else {
        stats_["equilibrio"] = qs(core::formatAmount(be.revenueNeeded));
        stats_["equilibrioNota"] = QStringLiteral("facturacion minima");
    }

    const int runway = fund.monthsOfRunway(1);
    if (runway == -1) {
        stats_["reserva"] = QStringLiteral("—");
        stats_["reservaNota"] = QStringLiteral("no estas consumiendo reservas");
    } else {
        stats_["reserva"] = QString::number(runway);
        stats_["reservaNota"] = QStringLiteral("a este ritmo");
    }

    const core::TicketStats ts = core::ticketStats(jobs, movements, currency_, from, to);
    stats_["ticket"] = qs(core::formatAmount(ts.averageIncome));
    stats_["ticketNota"] = QStringLiteral("sobre %1 trabajos").arg(ts.jobCount);

    const core::CollectionStats cs = core::collectionStats(movements, from, to);
    if (cs.sampled == 0) {
        stats_["cobro"] = QStringLiteral("—");
        stats_["cobroNota"] = QStringLiteral("todavia no hay cobros con fecha");
    } else {
        stats_["cobro"] = QString::number(cs.averageDays);
        stats_["cobroNota"] = QStringLiteral("sobre %1 cobros").arg(cs.sampled);
    }

    const core::Money ov = core::overhead(movements, currency_, from, to);
    stats_["estructura"] = qs(core::formatAmount(ov));
    if (flow.incomeAccrued.isZero()) {
        stats_["estructuraNota"] = QStringLiteral("Este mes no hubo ingresos para absorber la estructura.");
    } else {
        const double pct = static_cast<double>(ov.minor()) * 100.0 / static_cast<double>(flow.incomeAccrued.minor());
        stats_["estructuraNota"] = QStringLiteral("Se come el %1 de los ingresos del mes.")
                                       .arg(qs(core::formatBps(static_cast<int>(pct * 100.0))));
    }

    // --- Avisos ---
    alerts_.clear();
    const auto list = core::alerts(pockets, movements, jobs, currency_, today_,
                                   [](const core::Money& amount) {
                                       return core::formatAmount(amount);
                                   });
    for (const core::Alert& alert : list) {
        QVariantMap row;
        row["level"] = static_cast<int>(alert.level);
        row["title"] = qs(alert.title);
        row["detail"] = qs(alert.detail);
        alerts_.append(row);
    }

    emit dataChanged();
}

// ------------------------------------------------------------ Escrituras ---

QString AppBridge::saveMovement(const QVariantMap& draft) {
    if (!repository_) {
        return fatalError_;
    }
    const QString name = draft.value("name").toString().trimmed();
    if (name.isEmpty()) {
        return QStringLiteral("Falta decir que fue.");
    }

    const QString amountText = draft.value("amount").toString().trimmed();
    if (amountText.isEmpty()) {
        return QStringLiteral("Falta el monto.");
    }

    core::Money amount;
    try {
        amount = core::Money::parse(amountText.toStdString(), currency_);
    } catch (const std::exception&) {
        return QStringLiteral("Monto invalido.");
    }
    if (amount.isNegative() || amount.isZero()) {
        return QStringLiteral("El monto va en positivo; el signo lo pone el tipo.");
    }

    core::Movement movement;
    movement.kind = kindFromIndex(draft.value("kind").toInt());
    movement.name = name.toStdString();
    movement.amountMinor = amount.minor();

    try {
        movement.date = core::Date::fromIso(draft.value("date").toString().toStdString());
    } catch (const std::exception&) {
        return QStringLiteral("Fecha invalida.");
    }

    movement.pocketId = draft.value("pocketId").toString().toStdString();
    if (movement.pocketId.empty()) {
        return QStringLiteral("Elegi de que bolsillo sale.");
    }

    if (movement.kind == core::MovementKind::Traspaso) {
        movement.targetPocketId = draft.value("targetPocketId").toString().toStdString();
        if (movement.targetPocketId.empty()) {
            return QStringLiteral("Un traspaso necesita decir a donde va.");
        }
        if (movement.targetPocketId == movement.pocketId) {
            return QStringLiteral("Un traspaso necesita dos bolsillos distintos.");
        }
        movement.category = std::string(core::kUncategorized);
    } else {
        const QString category = draft.value("category").toString().trimmed();
        movement.category = category.isEmpty() ? std::string(core::kUncategorized)
                                               : category.toStdString();
        movement.jobId = draft.value("jobId").toString().toStdString();
        movement.settled = draft.value("settled", true).toBool();
        if (movement.kind == core::MovementKind::Gasto) {
            movement.spreadMonths = std::max(1, draft.value("spreadMonths", 1).toInt());
        }
    }

    movement.id = storage::newId();
    stamp(movement.hlc, movement.deviceId);

    try {
        repository_->save(movement);
    } catch (const std::exception& error) {
        return QString::fromUtf8(error.what());
    }

    afterLocalChange();
    return {};
}

QString AppBridge::removeMovement(const QString& id) {
    if (!repository_) {
        return fatalError_;
    }
    for (const core::Movement& movement : movementsData_) {
        if (qs(movement.id) != id) {
            continue;
        }
        try {
            repository_->remove(movement);
        } catch (const std::exception& error) {
            return QString::fromUtf8(error.what());
        }
        afterLocalChange();
        return {};
    }
    return QStringLiteral("Ese movimiento ya no esta.");
}

QStringList AppBridge::categoriesFor(int kind) const {
    if (!repository_) {
        return {};
    }
    const core::MovementKind wanted = kindFromIndex(kind);

    std::vector<std::pair<std::string, int>> counts;
    for (const core::Movement& movement : movementsData_) {
        if (movement.kind != wanted || movement.category.empty()) {
            continue;
        }
        const auto it = std::find_if(
            counts.begin(), counts.end(),
            [&movement](const auto& entry) { return entry.first == movement.category; });
        if (it == counts.end()) {
            counts.emplace_back(movement.category, 1);
        } else {
            ++it->second;
        }
    }
    std::stable_sort(counts.begin(), counts.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });

    QStringList out;
    for (const auto& [category, uses] : counts) {
        out << qs(category);
    }
    return out;
}

QString AppBridge::addPocket(const QString& name, int kind, const QString& opening) {
    if (!repository_) {
        return fatalError_;
    }
    if (name.trimmed().isEmpty()) {
        return QStringLiteral("El bolsillo necesita un nombre.");
    }

    core::Pocket pocket;
    pocket.name = name.trimmed().toStdString();
    pocket.kind = static_cast<core::PocketKind>(std::clamp(kind, 0, 3));
    try {
        const QString text = opening.trimmed().isEmpty() ? QStringLiteral("0") : opening.trimmed();
        pocket.openingMinor = core::Money::parse(text.toStdString(), currency_).minor();
    } catch (const std::exception&) {
        return QStringLiteral("El saldo inicial no se entiende.");
    }

    pocket.id = storage::newId();
    stamp(pocket.hlc, pocket.deviceId);
    try {
        repository_->save(pocket);
    } catch (const std::exception& error) {
        return QString::fromUtf8(error.what());
    }
    afterLocalChange();
    return {};
}

QString AppBridge::addJob(const QString& name, const QString& client) {
    if (!repository_) {
        return fatalError_;
    }
    if (name.trimmed().isEmpty()) {
        return QStringLiteral("El trabajo necesita un nombre.");
    }
    core::Job job;
    job.name = name.trimmed().toStdString();
    job.client = client.trimmed().toStdString();
    job.opened = today_;
    job.id = storage::newId();
    stamp(job.hlc, job.deviceId);
    try {
        repository_->save(job);
    } catch (const std::exception& error) {
        return QString::fromUtf8(error.what());
    }
    afterLocalChange();
    return {};
}

QString AppBridge::setJobClosed(const QString& id, bool closed) {
    if (!repository_) {
        return fatalError_;
    }
    for (core::Job job : jobsData_) {
        if (qs(job.id) != id) {
            continue;
        }
        if (job.closed == closed) {
            return {};
        }
        job.closed = closed;
        stamp(job.hlc, job.deviceId);
        try {
            repository_->save(job);
        } catch (const std::exception& error) {
            return QString::fromUtf8(error.what());
        }
        afterLocalChange();
        return {};
    }
    return QStringLiteral("Ese trabajo ya no esta.");
}

QString AppBridge::eraseAll() {
    if (!repository_) {
        return fatalError_;
    }
    try {
        const std::size_t borrados = repository_->deleteEverything();
        afterLocalChange();
        return QStringLiteral("Se borraron %1 registros.").arg(borrados);
    } catch (const std::exception& error) {
        return QStringLiteral("No se pudo borrar: ") + QString::fromUtf8(error.what());
    }
}

// ------------------------------------------------------- Cierre de mes ----
//
// El mismo calculo que ClosingPage en el escritorio, con el mismo nucleo. La
// unica diferencia es la forma: alla es una tabla ancha, aca son tarjetas que
// entran en una pantalla de telefono.

QStringList AppBridge::closingMonths() const {
    QStringList salida;
    // Del mas nuevo al mas viejo: el cierre que uno mira es el ultimo, no el
    // primero. summarizeByMonth los devuelve al reves.
    const auto meses = core::summarizeByMonth(pocketsData_, movementsData_, currency_);
    for (std::size_t i = meses.size(); i-- > 0;) {
        salida << qs(meses[i].label());
    }
    return salida;
}

QVariantMap AppBridge::closing(int index) const {
    QVariantMap salida;
    const auto meses = core::summarizeByMonth(pocketsData_, movementsData_, currency_);
    if (meses.empty()) {
        salida["vacio"] = true;
        salida["contra"] = QStringLiteral("Todavia no hay ningun mes con movimientos.");
        return salida;
    }

    // `index` viene de closingMonths(), que va al reves. Se traduce aca y no en
    // QML: una pantalla que tiene que saber en que orden guarda los datos el
    // motor es una pantalla que se rompe la proxima vez que ese orden cambie.
    const int ultimo = static_cast<int>(meses.size()) - 1;
    const int elegido = std::clamp(ultimo - index, 0, ultimo);
    const core::MonthSummary& mes = meses[static_cast<std::size_t>(elegido)];

    const core::Date desde = core::Date::fromYmd(mes.year, mes.month, 1);
    const core::Date hasta = desde.lastDayOfMonth();
    const core::CashFlow flujo = core::cashFlow(movementsData_, currency_, desde, hasta);

    salida["vacio"] = false;
    salida["etiqueta"] = qs(mes.label());
    salida["resultado"] = qs(core::formatAmount(flujo.result));
    salida["resultadoNegativo"] = flujo.result.isNegative();
    salida["facturado"] = qs(core::formatAmount(flujo.incomeAccrued));
    salida["costo"] = qs(core::formatAmount(flujo.cost));
    salida["caja"] = qs(core::formatAmount(flujo.cashDelta));
    salida["cajaNegativa"] = flujo.cashDelta.isNegative();

    // Lo que quedo por cobrar se mira AL CIERRE de ese mes, no hoy: preguntarlo
    // con la fecha de hoy contestaria otra pregunta.
    const auto balances = core::pocketBalances(pocketsData_, movementsData_, currency_, hasta);
    core::Money porCobrar = core::Money::zero(currency_);
    for (const core::PocketBalance& balance : balances) {
        porCobrar += balance.pendingIn;
    }
    salida["porCobrar"] = qs(core::formatAmount(porCobrar));

    if (elegido > 0) {
        const core::MonthSummary& anterior = meses[static_cast<std::size_t>(elegido) - 1];
        const core::Money diferencia = mes.result - anterior.result;
        QString texto = qs(core::formatAmount(diferencia));
        if (!diferencia.isNegative() && !diferencia.isZero()) {
            texto.prepend(QLatin1Char('+'));
        }
        salida["contra"] = QStringLiteral("Contra %1: %2 de resultado.")
                               .arg(qs(anterior.label()), texto);
    } else {
        salida["contra"] =
            QStringLiteral("Es el primer mes con movimientos: no hay contra que compararlo.");
    }

    QVariantList categorias;
    for (const core::CategoryTotal& ct : core::costByCategory(movementsData_, currency_, desde, hasta)) {
        QVariantMap fila;
        fila["etiqueta"] = ct.category.empty() ? QStringLiteral("Sin categoria")
                                               : qs(ct.category);
        fila["texto"] = qs(core::formatAmount(ct.total));
        categorias.append(fila);
    }
    salida["categorias"] = categorias;
    return salida;
}

QString AppBridge::reconcile(const QString& pocketId, const QString& realAmount) {
    if (!repository_) {
        return fatalError_;
    }
    const auto balances =
        core::pocketBalances(pocketsData_, movementsData_, currency_, today_);

    const auto it = std::find_if(balances.begin(), balances.end(),
                                 [&pocketId](const core::PocketBalance& balance) {
                                     return qs(balance.pocketId) == pocketId;
                                 });
    if (it == balances.end()) {
        return QStringLiteral("Ese bolsillo ya no esta.");
    }

    core::Money real;
    try {
        real = core::Money::parse(realAmount.trimmed().toStdString(), currency_);
    } catch (const std::exception&) {
        return QStringLiteral("Ese saldo no se entiende.");
    }

    const core::Money delta = real - it->balance;
    if (delta.isZero()) {
        return QStringLiteral("Ya cuadraba: no hizo falta anotar nada.");
    }

    core::Movement adjustment;
    adjustment.date = today_;
    adjustment.name = "Ajuste de " + it->name;
    adjustment.kind =
        delta.isNegative() ? core::MovementKind::Gasto : core::MovementKind::Ingreso;
    adjustment.amountMinor = delta.isNegative() ? -delta.minor() : delta.minor();
    adjustment.pocketId = it->pocketId;
    adjustment.category = std::string(core::kAdjustment);
    adjustment.id = storage::newId();
    stamp(adjustment.hlc, adjustment.deviceId);

    try {
        repository_->save(adjustment);
    } catch (const std::exception& error) {
        return QString::fromUtf8(error.what());
    }
    afterLocalChange();
    return {};
}

// -------------------------------------------------------------- Archivos ---

QString AppBridge::suggestedFileName() const {
    return QStringLiteral("dakelabs-%1.jsonl").arg(qs(today_.toIso()));
}

QString AppBridge::exportTo(const QUrl& url) {
    if (!repository_) {
        return fatalError_;
    }
    try {
        const int written = storage::exportAll(*repository_, pathFromUrl(url));
        return QStringLiteral("Se exportaron %1 registros.").arg(written);
    } catch (const std::exception& error) {
        return QStringLiteral("No se pudo exportar: ") + QString::fromUtf8(error.what());
    }
}

QString AppBridge::importFrom(const QUrl& url) {
    if (!repository_) {
        return fatalError_;
    }
    try {
        const auto report = storage::importFile(*repository_, pathFromUrl(url));
        afterLocalChange();
        QString message = QStringLiteral("Entraron %1 de %2 registros")
                              .arg(report.applied)
                              .arg(report.total);
        if (report.skippedOlder > 0) {
            message += QStringLiteral("; %1 ya estaban al dia").arg(report.skippedOlder);
        }
        if (report.malformed > 0) {
            message += QStringLiteral("; %1 lineas no se entendieron").arg(report.malformed);
        }
        return message + QStringLiteral(".");
    } catch (const std::exception& error) {
        return QStringLiteral("No se pudo importar: ") + QString::fromUtf8(error.what());
    }
}

// ---------------------------------------------------------------- Nube ---

bool AppBridge::signedIn() const {
    return supabase_ && supabase_->isSignedIn();
}

QString AppBridge::userEmail() const {
    return supabase_ ? supabase_->userEmail() : QString();
}

int AppBridge::pendingCount() const {
    return repository_ ? repository_->pendingOutboxCount() : 0;
}

bool AppBridge::syncing() const {
    return syncEngine_ && syncEngine_->isRunning();
}

void AppBridge::signIn(const QString& email, const QString& password) {
    if (supabase_) {
        supabase_->signInWithPassword(email, password);
    }
}

void AppBridge::signOut() {
    if (supabase_) {
        supabase_->signOut();
    }
    if (repository_) {
        repository_->setSetting(QStringLiteral("sync.refresh_token"), QString());
    }
}

void AppBridge::afterLocalChange() {
    reload();
    if (supabase_ && supabase_->isSignedIn()) {
        // start() sobre un temporizador andando lo reinicia desde cero: esa es
        // toda la agrupacion. No hace falta contar cambios ni acumular nada.
        autoSyncTimer_->start();
    }
}

void AppBridge::runAutoSync() {
    if (!supabase_ || !supabase_->isSignedIn() || !syncEngine_) {
        return;
    }
    if (syncEngine_->isRunning()) {
        // Volver a esperar en vez de encolar una segunda. Lo que se guardo
        // recien ya esta en la cola de salida: lo sube esta pasada si llega a
        // tiempo, o la proxima.
        autoSyncTimer_->start();
        return;
    }
    syncEngine_->sync();
}

void AppBridge::sync() {
    if (!supabase_ || !supabase_->isSignedIn()) {
        cloudStatus_ = QStringLiteral("Conectate primero.");
        emit cloudChanged();
        return;
    }
    if (syncEngine_) {
        syncEngine_->sync();
    }
}

QString AppBridge::formatMinor(qlonglong minor) const {
    return qs(core::formatAmount(core::Money::fromMinor(minor, currency_)));
}

QString AppBridge::relativeDate(const QString& isoDate) const {
    try {
        const core::Date date = core::Date::fromIso(isoDate.toStdString());
        const std::int64_t days = today_.toEpochDays() - date.toEpochDays();
        if (days == 0) {
            return QStringLiteral("hoy");
        }
        if (days == 1) {
            return QStringLiteral("ayer");
        }
        return isoDate;
    } catch (const std::exception&) {
        return isoDate;
    }
}

} // namespace dake::mobile
