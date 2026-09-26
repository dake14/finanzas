//
// ui/preview.cpp — banco de pruebas visual.
//
// Renderiza una pantalla a un PNG sin abrir ninguna ventana, con el complemento
// "offscreen" de Qt. Existe por una razon concreta: verificar la interfaz
// tomando capturas del escritorio obliga a traer la aplicacion al frente y a
// hacer clics sinteticos, y cualquier utilidad que se ponga encima (una
// notificacion, un panel del fabricante) desvia esos clics a la ventana
// equivocada.
//
// Aca no hay foco que robar ni clic que desviar: se arma el widget, se le dan
// datos y se guarda lo que pinta.
//
// Uso:  dake_uipreview <pantalla|todas> <salida.png|carpeta> [ancho] [alto]
//
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>

#include <iostream>
#include <memory>
#include <string>

#include "dake/core/accounts.hpp"
#include "dake/core/demo.hpp"
#include "dake/storage/database.hpp"
#include "dake/storage/quotefolder.hpp"
#include "dake/storage/repository.hpp"
#include "dake/core/repairs.hpp"
#include "capturewidget.hpp"
#include "capturewindow.hpp"
#include "pages.hpp"
#include "theme.hpp"

namespace {

[[nodiscard]] dake::ui::Snapshot demoSnapshot() {
    const auto currency = dake::core::Currency::usd();
    const auto data = dake::core::realCaseAugust2026(currency);

    dake::ui::Snapshot snapshot;
    snapshot.pockets = data.pockets;
    snapshot.jobs = data.jobs;
    snapshot.movements = data.movements;
    snapshot.currency = currency;
    // Se mira desde el 31 de agosto: es el corte que hace comparables los
    // numeros de esta pantalla con los que muestra hoy la aplicacion real.
    snapshot.today = dake::core::Date::fromYmd(2026, 8, 31);
    snapshot.categories = dake::core::inferCategories(snapshot.movements, snapshot.pockets, {});

    // Reparaciones de muestra, SOLO para el banco visual: el caso de agosto
    // no tiene fichas y los reportes de rentabilidad saldrian vacios. Tres
    // GPU que tardan mas de lo presupuestado, dos laptops sanas y una placa.
    snapshot.costs = dake::core::CostSettings{15'00, 3000, 4'00};
    int n = 0;
    auto sample = [&](dake::core::RepairType type, const char* device, const char* client,
                      std::int64_t price, int est, int real, std::int64_t parts,
                      const char* delivered, dake::core::RepairStatus status) {
        ++n;
        dake::core::Job job;
        job.id = "demo-job-" + std::to_string(n);
        job.name = device;
        job.client = client;
        job.opened = dake::core::Date::fromIso(delivered).addDays(-4);
        job.closed = status == dake::core::RepairStatus::Cobrada;
        snapshot.jobs.push_back(job);
        dake::core::Repair r;
        r.jobId = job.id;
        r.orderNo = "R-00" + std::to_string(40 + n);
        r.device = device;
        r.type = type;
        r.status = status;
        r.priceMinor = price;
        r.estMinutes = est;
        if (status != dake::core::RepairStatus::EnProceso) {
            r.realMinutes = real;
            r.delivered = dake::core::Date::fromIso(delivered);
        }
        r.received = job.opened;
        r.consumablesMinor = 3'00;
        snapshot.repairs.push_back(r);
        dake::core::RepairPart p;
        p.id = "demo-part-" + std::to_string(n);
        p.jobId = job.id;
        p.name = "Repuesto";
        p.costMinor = parts;
        snapshot.parts.push_back(p);
    };
    using dake::core::RepairStatus;
    using dake::core::RepairType;
    sample(RepairType::GPU, "RTX 3080", "Juan Perez", 90'00, 120, 190, 12'00, "2026-08-20", RepairStatus::Cobrada);
    sample(RepairType::GPU, "RTX 3070", "Ana", 85'00, 120, 170, 10'00, "2026-08-10", RepairStatus::Cobrada);
    sample(RepairType::GPU, "RX 6700", "Luis", 95'00, 150, 200, 15'00, "2026-07-28", RepairStatus::Cobrada);
    sample(RepairType::Laptop, "Asus X556U", "Josue Rodriguez", 60'00, 90, 80, 5'00, "2026-08-25", RepairStatus::Cobrada);
    sample(RepairType::Laptop, "HP 15", "Maria", 55'00, 90, 75, 0, "2026-08-02", RepairStatus::Cobrada);
    sample(RepairType::Laptop, "Lenovo T480", "Pedro", 70'00, 90, 95, 8'00, "2026-07-15", RepairStatus::Entregada);
    sample(RepairType::PlacaMadre, "Asus B450-F", "Sr. Diovis", 40'00, 180, 240, 6'00, "2026-08-18", RepairStatus::Cobrada);
    sample(RepairType::GPU, "RTX 2060", "Carla", 80'00, 120, 0, 0, "2026-08-30", RepairStatus::EnProceso);
    for (auto tpl : dake::core::defaultTemplates()) {
        tpl.id = "demo-tpl-" + tpl.name;
        snapshot.templates.push_back(tpl);
    }
    return snapshot;
}

[[nodiscard]] std::unique_ptr<QWidget> build(const QString& screen,
                                             const dake::ui::Snapshot& snapshot) {
    if (screen == QLatin1String("hoy")) {
        auto page = std::make_unique<dake::ui::TodayPage>();
        page->setSnapshot(snapshot);
        return page;
    }
    if (screen.startsWith(QLatin1String("reparaciones"))) {
        // "reparaciones:R-0041" abre la ficha de esa orden.
        auto page = std::make_unique<dake::ui::RepairsPage>();
        page->setSnapshot(snapshot);
        const QString order = screen.section(QLatin1Char(':'), 1);
        for (const auto& repair : snapshot.repairs) {
            if (QString::fromStdString(repair.orderNo) == order) {
                page->selectRepair(repair.jobId);
            }
        }
        return page;
    }
    if (screen == QLatin1String("movimientos")) {
        auto page = std::make_unique<dake::ui::MovementsPage>();
        page->setSnapshot(snapshot);
        return page;
    }
    if (screen == QLatin1String("bolsillos")) {
        auto page = std::make_unique<dake::ui::PocketsPage>();
        page->setSnapshot(snapshot);
        return page;
    }
    if (screen.startsWith(QLatin1String("captura"))) {
        // "captura:25 almuerzo ayer" muestra la ventana mini con esa linea.
        auto window = std::make_unique<dake::ui::CaptureWindow>();
        window->capture()->setSnapshot(snapshot);
        const QString line = screen.section(QLatin1Char(':'), 1);
        window->capture()->setInput(line.isEmpty() ? QStringLiteral("25 almuerzo ayer") : line);
        window->setAttribute(Qt::WA_TranslucentBackground, false);
        return window;
    }
    if (screen.startsWith(QLatin1String("reportes"))) {
        // "reportes:1" abre la segunda pestaña.
        auto page = std::make_unique<dake::ui::ReportsPage>();
        page->setSnapshot(snapshot);
        page->showTab(screen.section(QLatin1Char(':'), 1).toInt());
        return page;
    }
    if (screen == QLatin1String("ajustes")) {
        auto page = std::make_unique<dake::ui::SettingsPage>();
        page->setSnapshot(snapshot);
        return page;
    }
    if (screen == QLatin1String("cierre")) {
        auto page = std::make_unique<dake::ui::ClosingPage>();
        page->setSnapshot(snapshot);
        return page;
    }
    return nullptr;
}

bool render(const QString& screen, const QString& output, int width, int height,
            const dake::ui::Snapshot& snapshot) {
    std::unique_ptr<QWidget> widget = build(screen, snapshot);
    if (widget == nullptr) {
        std::cout << "Pantalla desconocida: " << screen.toStdString() << "\n";
        return false;
    }

    widget->resize(width, height);
    widget->show();
    // Sin esta vuelta al bucle de eventos el layout todavia no se aplico y la
    // captura sale con todos los widgets apilados en la esquina.
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const QPixmap shot = widget->grab();
    if (!shot.save(output)) {
        std::cout << "No se pudo guardar: " << output.toStdString() << "\n";
        return false;
    }
    std::cout << "Guardado " << output.toStdString() << " (" << shot.width() << "x"
              << shot.height() << ")\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    // El complemento se fija antes de construir QApplication; despues ya es
    // tarde.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // El offscreen no hereda las fuentes del sistema como si hace el de
    // Windows: sin esto cada letra se dibuja como un rectangulo vacio y la
    // captura no sirve para leer nada.
#ifdef Q_OS_WIN
    if (qgetenv("QT_QPA_FONTDIR").isEmpty()) {
        qputenv("QT_QPA_FONTDIR", "C:/Windows/Fonts");
    }
#endif

    QApplication app(argc, argv);
    app.setStyleSheet(dake::ui::theme::styleSheet());

    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() < 2 || (arguments.size() < 3 && arguments.at(1) != QLatin1String("cotizaciones"))) {
        std::cout << "Uso: dake_uipreview <pantalla|todas> <salida.png|carpeta> "
                     "[ancho] [alto]\n"
                  << "Pantallas: hoy | reparaciones | movimientos | bolsillos | cierre | reportes | ajustes | todas\n";
        return 2;
    }

    // "cotizaciones [carpeta]": lista lo que Finanzas entiende de cada
    // documento de DakeLabs Cotizaciones. Solo lee; sirve para comparar contra
    // lo que muestra Cotizaciones.
    if (arguments.at(1) == QLatin1String("cotizaciones")) {
        const QString folder =
            arguments.size() > 2 ? arguments.at(2) : dake::storage::defaultQuoteFolder();
        const auto read = dake::storage::readQuoteFolder(folder);
        std::cout << "carpeta " << folder.toStdString() << (read.folderFound ? "" : " (no existe)")
                  << "\n";
        for (const auto& d : read.docs) {
            std::cout << d.number << "\t" << d.status << "\tbase=" << d.baseMinor
                      << "\tabono=" << d.depositMinor << "\tsaldo=" << d.balanceMinor()
                      << "\tpagado=" << (d.paid ? d.paid->toIso() : std::string("-"))
                      << "\torigen=" << (d.originId.empty() ? std::string("-") : d.originId.substr(0, 8))
                      << "\t" << d.device << "\n";
        }
        for (const QString& error : read.errors) {
            std::cout << "ERROR " << error.toStdString() << "\n";
        }
        return 0;
    }

    // "plan <base> [carpeta]": lo que haria la importacion de Cotizaciones
    // sobre esa base, sin aplicar nada. Pensado para correrlo sobre una COPIA
    // de la base real: abrirla la migra.
    if (arguments.at(1) == QLatin1String("plan") && arguments.size() > 2) {
        dake::storage::Database db(arguments.at(2));
        dake::storage::Repository repository(db);
        const QString folder =
            arguments.size() > 3 ? arguments.at(3) : dake::storage::defaultQuoteFolder();
        const auto read = dake::storage::readQuoteFolder(folder);
        dake::core::QuoteContext context;
        context.repairs = repository.loadRepairs();
        context.jobs = repository.loadJobs();
        context.movements = repository.loadMovements();
        context.parts = repository.loadRepairParts();
        context.pocketId = "(bolsillo de cobros)";
        context.category = "Reparaciones";
        std::cout << "movimientos en la base: " << context.movements.size()
                  << ", trabajos: " << context.jobs.size() << "\n";
        for (const auto& m : context.movements) {
            if (m.kind == dake::core::MovementKind::Ingreso) {
                std::cout << "  ingreso " << m.date.toIso() << "\t" << m.amountMinor << "\t"
                          << (m.settled ? "cobrado" : "por cobrar") << "\t" << m.name
                          << (m.jobId.empty() ? "" : "  [con trabajo]") << "\n";
            }
        }
        for (const auto& plan : dake::core::planQuotes(read.docs, context)) {
            const char* decision = plan.decision == dake::core::QuoteDecision::Importar ? "IMPORTAR"
                                   : plan.decision == dake::core::QuoteDecision::Esperar ? "ESPERA"
                                                                                          : "nada";
            std::cout << plan.number << "\t" << decision;
            switch (plan.hold) {
                case dake::core::QuoteHold::Duplicado: std::cout << "\tduplicado de " << plan.relatedNumber; break;
                case dake::core::QuoteHold::YaAnotado: std::cout << "\tya anotado: " << plan.candidateMovementId; break;
                case dake::core::QuoteHold::EnCorreccion: std::cout << "\ten correccion"; break;
                case dake::core::QuoteHold::SinMonto: std::cout << "\tsin monto"; break;
                case dake::core::QuoteHold::Ninguno: break;
            }
            if (plan.repair) {
                std::cout << "\treparacion " << plan.repair->jobId << (plan.newJob ? " (nueva)" : "")
                          << " " << dake::core::toString(plan.repair->status) << " tipo "
                          << dake::core::toString(plan.repair->type);
            }
            for (const auto& m : plan.incomes) {
                std::cout << "\n\t  ingreso " << m.id << " " << m.amountMinor
                          << (m.settled ? " cobrado" : " por cobrar");
            }
            std::cout << "\n";
        }
        return 0;
    }

    const QString screen = arguments.at(1);
    const QString output = arguments.at(2);
    const int width = arguments.size() > 3 ? arguments.at(3).toInt() : 1280;
    const int height = arguments.size() > 4 ? arguments.at(4).toInt() : 1400;

    const dake::ui::Snapshot snapshot = demoSnapshot();

    if (screen == QLatin1String("todas")) {
        QDir().mkpath(output);
        bool ok = true;
        for (const QString& one : {QStringLiteral("hoy"), QStringLiteral("reparaciones"),
                                   QStringLiteral("movimientos"), QStringLiteral("bolsillos"),
                                   QStringLiteral("cierre"), QStringLiteral("reportes"),
                                   QStringLiteral("ajustes")}) {
            ok = render(one, output + QLatin1Char('/') + one + QStringLiteral(".png"), width,
                        height, snapshot) &&
                 ok;
        }
        return ok ? 0 : 1;
    }

    QDir().mkpath(QFileInfo(output).absolutePath());
    return render(screen, output, width, height, snapshot) ? 0 : 1;
}
