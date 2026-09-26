// tests/uitest.cpp — la interfaz de verdad, manejada solo con el teclado.
//
// Arma la ventana principal sobre una base temporal y la maneja como la
// manejaria alguien sin mouse: escribe en la captura, abre una reparacion con
// Ctrl+R, la entrega con Ctrl+E. Despues mira la base para ver que quedo.
//
// Corre SIN PANTALLA (complemento offscreen) y las teclas van directo a los
// widgets de esta aplicacion con QTest. Nunca al teclado del sistema: una
// prueba que simula teclas globales escribe en la ventana que tenga el foco,
// que puede ser cualquier otra, y eso ya paso una vez.
//
// Un vigilante corta la prueba a los 120 segundos: un dialogo modal que nadie
// contesta la dejaria colgada para siempre.

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include "capturewidget.hpp"
#include "dake/storage/database.hpp"
#include "dake/storage/repository.hpp"
#include "mainwindow.hpp"
#include "pages.hpp"

namespace {

int gFailures = 0;

void check(bool condition, const std::string& what) {
    std::printf("%s  %s\n", condition ? "ok  " : "FALLA", what.c_str());
    if (!condition) {
        ++gFailures;
    }
}

using namespace dake;

/// Hace `action` con el proximo dialogo modal que aparezca. El dialogo se abre
/// con exec(), que no vuelve hasta que se cierra: la unica forma de manejarlo
/// es dejar esto programado antes de abrirlo.
void onNextDialog(std::function<void(QWidget*)> action) {
    auto* timer = new QTimer();
    timer->setInterval(30);
    QObject::connect(timer, &QTimer::timeout, [timer, action] {
        if (QWidget* dialog = QApplication::activeModalWidget()) {
            timer->stop();
            timer->deleteLater();
            std::printf("      (dialogo: %s)\n", dialog->windowTitle().toStdString().c_str());
            // Sin pantalla, un dialogo nuevo no toma el foco solo (en Windows
            // si). Se lo activa como lo haria el sistema.
            dialog->activateWindow();
            (void)QTest::qWaitForWindowActive(dialog);
            action(dialog);
        }
    });
    timer->start();
}

/// Escribe en el campo que el dialogo tiene enfocado y aprieta Enter. Se usa
/// el foco DEL DIALOGO y no el de la aplicacion: sin pantalla, la ventana
/// activa no cambia al abrir un dialogo, pero cada ventana recuerda cual de
/// sus campos tiene el foco, y ahi es donde irian las teclas en Windows.
void typeAndEnter(QWidget* dialog, const QString& text) {
    QWidget* target = dialog->focusWidget();
    const auto* edit = qobject_cast<QLineEdit*>(target);
    std::printf("      escribo '%s' en [%s]\n", text.toStdString().c_str(),
                edit != nullptr ? edit->placeholderText().toStdString().c_str() : "?");
    if (target == nullptr) {
        check(false, "el dialogo no tiene ningun campo enfocado");
        return;
    }
    QTest::keyClicks(target, text);
    QTest::keyClick(dialog->focusWidget(), Qt::Key_Return);
}

void typeTabThenEnter(QWidget* dialog, const QString& first, const QString& second) {
    QTest::keyClicks(dialog->focusWidget(), first);
    QTest::keyClick(dialog->focusWidget(), Qt::Key_Tab);
    QTest::keyClicks(dialog->focusWidget(), second);
    QTest::keyClick(dialog->focusWidget(), Qt::Key_Return);
}

void settle() {
    for (int i = 0; i < 20; ++i) {
        QCoreApplication::processEvents();
        QTest::qWait(10);
    }
}

/// Despues de cerrar un dialogo, Windows le devuelve la actividad a la ventana
/// que lo abrio. Sin pantalla no pasa solo, y sin ventana activa ningun atajo
/// de ventana responde.
void reactivate(QWidget* window) {
    settle();
    window->activateWindow();
    (void)QTest::qWaitForWindowActive(window);
}

[[nodiscard]] const core::Repair* findRepair(const std::vector<core::Repair>& repairs,
                                             const std::string& device) {
    for (const core::Repair& r : repairs) {
        if (r.device == device) return &r;
    }
    return nullptr;
}


[[nodiscard]] const core::Movement* findMovement(const std::vector<core::Movement>& movements,
                                                 const std::string& id) {
    for (const core::Movement& m : movements) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

void writeDoc(const QString& folder, const QString& name, const QByteArray& json) {
    QDir().mkpath(folder + QStringLiteral("/documentos"));
    // Como Cotizaciones: a un temporal y despues renombrado encima.
    const QString target = folder + QStringLiteral("/documentos/") + name;
    QFile tmp(target + QStringLiteral(".tmp"));
    tmp.open(QIODevice::WriteOnly);
    tmp.write(json);
    tmp.close();
    QFile::remove(target);
    QFile::rename(target + QStringLiteral(".tmp"), target);
}

[[nodiscard]] QByteArray informe(const char* id, const char* number, const char* status,
                                 const char* client, const char* device, int unit,
                                 const char* origin, bool paid) {
    QByteArray json = R"({"id":"ID","numero":"NUM","tipo":"informe","forma":"servicio","estado":"EST",
      "fechaEmision":"2026-09-19","fechaEntrega":"2026-09-19",
      "clienteCongelado":{"nombre":"CLI"},"equipo":{"descripcion":"DEV","fechaIngreso":"2026-09-10"},
      "categorias":[{"nombre":"Mano de obra","lineas":[{"concepto":"Reparacion","cantidad":1,"valorUnitario":UNIT}]}],
      "descuento":null,"abono":0,"origenId":ORIG,"historial":[HIST]})";
    json.replace("ID", id).replace("NUM", number).replace("EST", status).replace("CLI", client)
        .replace("DEV", device).replace("UNIT", QByteArray::number(unit))
        .replace("ORIG", origin == nullptr ? QByteArray("null") : QByteArray("\"") + origin + "\"")
        .replace("HIST", paid ? QByteArray(R"({"fecha":"2026-09-22","tipo":"estado","detalle":"Pagado"})")
                              : QByteArray());
    return json;
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    qputenv("QT_QPA_PLATFORM", "offscreen");
    if (qgetenv("QT_QPA_FONTDIR").isEmpty()) {
        qputenv("QT_QPA_FONTDIR", "C:/Windows/Fonts");
    }

    QTemporaryDir temp;
    const QString path = temp.path() + QStringLiteral("/interfaz.db");
    qputenv("DAKE_TEST_DB_PATH", path.toLocal8Bit());

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("dake-uitest"));

    QTimer::singleShot(120000, [] {
        std::printf("FALLA  la prueba no termino en 120 s: algo quedo esperando\n");
        if (QWidget* modal = QApplication::activeModalWidget()) {
            std::printf("       quedo abierto: '%s'\n", modal->windowTitle().toStdString().c_str());
            if (auto* box = qobject_cast<QMessageBox*>(modal)) {
                std::printf("       dice: %s\n", box->text().toStdString().c_str());
            }
            for (const QLabel* label : modal->findChildren<QLabel*>()) {
                if (label->isVisible() && !label->text().isEmpty()) {
                    std::printf("       etiqueta: %s\n", label->text().left(90).toStdString().c_str());
                }
            }
            for (const QLineEdit* edit : modal->findChildren<QLineEdit*>()) {
                std::printf("       campo [%s] = '%s'\n", edit->placeholderText().toStdString().c_str(),
                            edit->text().toStdString().c_str());
            }
            QWidget* focus = QApplication::focusWidget();
            std::printf("       foco en: %s\n",
                        focus != nullptr ? focus->metaObject()->className() : "(nada)");
        }
        std::fflush(stdout);
        std::_Exit(2);
    });

    std::printf("Banco de pruebas — la interfaz, sin mouse\n(%s)\n", path.toStdString().c_str());

    // Hermetica: una carpeta de Cotizaciones vacia y propia. Sin esto la
    // ventana leeria la carpeta real de Documentos.
    {
        storage::Database setup(path);
        storage::Repository repo(setup);
        repo.setSetting(QStringLiteral("cot.carpeta"), temp.path() + QStringLiteral("/sin-cotizaciones"));
    }

    ui::MainWindow window(path);
    window.show();
    check(QTest::qWaitForWindowExposed(&window), "la ventana principal se abre");
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window);

    storage::Database db(path);
    storage::Repository repository(db);

    // --- Anotar desde Hoy ----------------------------------------------------
    std::printf("\n[anotar desde Hoy]\n");
    {
        auto* input = window.findChild<QLineEdit*>(QStringLiteral("CaptureInput"));
        check(input != nullptr, "la linea de captura esta en Hoy");
        if (input != nullptr) {
            input->setFocus();
            QTest::keyClicks(input, QStringLiteral("25 almuerzo ayer"));
            QTest::keyClick(input, Qt::Key_Return);
            settle();
            bool found = false;
            for (const core::Movement& m : repository.loadMovements()) {
                if (m.name == "almuerzo" && m.amountMinor == 25'00 &&
                    m.kind == core::MovementKind::Gasto) {
                    found = true;
                }
            }
            check(found, "'25 almuerzo ayer' + Enter guarda un gasto de 25,00");
            check(input->text().isEmpty(), "y la linea queda vacia para el siguiente");
        }
    }

    // --- Reparacion nueva con Ctrl+R ------------------------------------------
    std::printf("\n[reparacion nueva, entregada y cobrada con el teclado]\n");
    {
        onNextDialog([](QWidget* dialog) {
            typeAndEnter(dialog, QStringLiteral("gpu"));
            typeAndEnter(dialog, QStringLiteral("Juan Perez"));
            typeAndEnter(dialog, QStringLiteral("RTX 3080"));
        });
        QTest::keyClick(&window, Qt::Key_R, Qt::ControlModifier);
        reactivate(&window);

        auto repairs = repository.loadRepairs();
        const core::Repair* created = findRepair(repairs, "RTX 3080");
        check(created != nullptr, "Ctrl+R, gpu, cliente, equipo: la reparacion existe");
        if (created != nullptr) {
            check(created->type == core::RepairType::GPU, "de tipo GPU, por la plantilla");
            check(created->orderNo == "R-0001", "con el primer numero de orden");
            check(created->estMinutes == 120, "con las horas de la plantilla");
            check(created->status == core::RepairStatus::EnProceso, "en proceso");
        }
        check(repository.timingMedian(QStringLiteral("reparacion")) >= 0,
              "y el alta quedo cronometrada");

        onNextDialog([](QWidget* dialog) {
            typeTabThenEnter(dialog, QStringLiteral("2,5"), QStringLiteral("90"));
        });
        QTest::keyClick(&window, Qt::Key_E, Qt::ControlModifier);
        reactivate(&window);

        repairs = repository.loadRepairs();
        created = findRepair(repairs, "RTX 3080");
        check(created != nullptr && created->status == core::RepairStatus::Cobrada,
              "Ctrl+E, horas, precio, Enter: entregada y cobrada");
        check(created != nullptr && created->realMinutes == 150, "con 2,5 horas reales");
        bool income = false;
        for (const core::Movement& m : repository.loadMovements()) {
            if (created != nullptr && m.jobId == created->jobId &&
                m.kind == core::MovementKind::Ingreso && m.amountMinor == 90'00 && m.settled &&
                m.settledDate.has_value()) {
                income = true;
            }
        }
        check(income, "y el ingreso de 90,00 cobrado se creo solo");
        bool closed = false;
        for (const core::Job& j : repository.loadJobs()) {
            if (created != nullptr && j.id == created->jobId) closed = j.closed;
        }
        check(closed, "el trabajo que ve el telefono queda cerrado");
    }

    // --- El cobro anotado en la captura cierra la reparacion --------------------
    std::printf("\n[el cobro anotado en la captura cierra la reparacion]\n");
    {
        onNextDialog([](QWidget* dialog) {
            typeAndEnter(dialog, QStringLiteral("laptop"));
            typeAndEnter(dialog, QStringLiteral("Ana"));
            typeAndEnter(dialog, QStringLiteral("Asus X556U"));
        });
        QTest::keyClick(&window, Qt::Key_R, Qt::ControlModifier);
        reactivate(&window);

        auto* input = window.findChild<QLineEdit*>(QStringLiteral("CaptureInput"));
        if (input != nullptr) {
            QTest::keyClick(&window, Qt::Key_1, Qt::ControlModifier);
            input->setFocus();
            auto* job = input->parentWidget()->findChild<QComboBox*>(QStringLiteral("CaptureJob"));
            QTest::keyClicks(input, QStringLiteral("12 pasta asus"));
            check(job != nullptr && !job->isVisibleTo(input->parentWidget()),
                  "un gasto no pregunta la reparacion, aunque la nombre");
            input->clear();
            QTest::keyClicks(input, QStringLiteral("60 cobro asus"));
            check(job != nullptr && job->isVisibleTo(input->parentWidget()), "un cobro si");
            QTest::keyClick(input, Qt::Key_Return);
            settle();
        }
        const auto repairs = repository.loadRepairs();
        const core::Repair* asus = findRepair(repairs, "Asus X556U");
        check(asus != nullptr, "la segunda reparacion existe");
        check(asus != nullptr && asus->status == core::RepairStatus::Cobrada,
              "'60 cobro asus' la deja cobrada sin tocar la ficha");
    }


    // --- DakeLabs Cotizaciones -----------------------------------------------
    //
    // Otra base y otra ventana, con una carpeta sintetica: una cotizacion
    // aceptada, su informe entregado, y el Macbook dos veces.
    std::printf("\n[DakeLabs Cotizaciones]\n");
    {
        const QString quotesPath = temp.path() + QStringLiteral("/cotizaciones.db");
        const QString folder = temp.path() + QStringLiteral("/DakeLabs Cotizaciones");
        writeDoc(folder, QStringLiteral("COT-2026-001.json"), R"({"id":"c1","numero":"COT-2026-001",
          "tipo":"cotizacion","estado":"aceptada","fechaEmision":"2026-09-01",
          "clienteCongelado":{"nombre":"Josue Rodríguez"},"equipo":{"descripcion":"asus x556U","fechaIngreso":"2026-09-01"},
          "categorias":[],"descuento":null,"abono":0,"origenId":null,"historial":[]})");
        writeDoc(folder, QStringLiteral("INF-2026-004.json"),
                 informe("i4", "INF-2026-004", "entregado", "Josue Rodríguez", "asus x556U", 1000, "c1", false));
        writeDoc(folder, QStringLiteral("INF-2026-001.json"),
                 informe("i1", "INF-2026-001", "pagado", "Sr. Galván", "Macbook M1", 6983, nullptr, true));
        writeDoc(folder, QStringLiteral("INF-2026-002.json"),
                 informe("i2", "INF-2026-002", "pagado", "Sr. Galván", "Macbook M1", 6982, nullptr, true));
        {
            storage::Database setup(quotesPath);
            storage::Repository repo(setup);
            repo.setSetting(QStringLiteral("cot.carpeta"), folder);
        }
        qputenv("DAKE_TEST_DB_PATH", quotesPath.toLocal8Bit());

        ui::MainWindow quotesWindow(quotesPath);
        quotesWindow.show();
        (void)QTest::qWaitForWindowExposed(&quotesWindow);
        quotesWindow.activateWindow();
        settle();

        storage::Database qdb(quotesPath);
        storage::Repository qrepo(qdb);
        auto movements = qrepo.loadMovements();
        const core::Movement* saldo = findMovement(movements, "cot-i4-saldo");
        check(saldo != nullptr && saldo->amountMinor == 10'00 && !saldo->settled,
              "al arrancar: el informe entregado es un ingreso por cobrar de 10,00");
        check(saldo != nullptr && saldo->jobId == "cot-c1",
              "sobre la reparacion que abrio la cotizacion");
        const auto quoteRepairs = qrepo.loadRepairs();
        const core::Repair* asus = findRepair(quoteRepairs, "asus x556U");
        if (asus != nullptr) {
            std::printf("      (asus: %s, %s)\n", std::string(core::toString(asus->status)).c_str(),
                        asus->orderNo.c_str());
        }
        check(asus != nullptr && asus->status == core::RepairStatus::Entregada &&
                  asus->orderNo == "INF-2026-004",
              "la reparacion queda entregada, con el numero del informe");
        const core::Movement* mac = findMovement(movements, "cot-i1-saldo");
        check(mac != nullptr && mac->settled && mac->settledDate == (core::Date{2026, 9, 22}),
              "el informe pagado entra cobrado, con la fecha del evento Pagado");
        check(findMovement(movements, "cot-i2-saldo") == nullptr,
              "el repetido no entra: espera que decidas");

        // Releer no duplica nada.
        const std::size_t before = qrepo.loadMovements().size();
        emit quotesWindow.findChild<ui::SettingsPage*>()->quoteReadRequested();
        settle();
        check(qrepo.loadMovements().size() == before, "leer la carpeta otra vez no agrega nada");

        // Cotizaciones marca el informe pagado: el archivo cambia y Finanzas se entera sola.
        writeDoc(folder, QStringLiteral("INF-2026-004.json"),
                 informe("i4", "INF-2026-004", "pagado", "Josue Rodríguez", "asus x556U", 1000, "c1", true));
        bool settledNow = false;
        for (int i = 0; i < 60 && !settledNow; ++i) {
            QTest::qWait(100);
            const auto now = qrepo.loadMovements();
            const core::Movement* m = findMovement(now, "cot-i4-saldo");
            settledNow = m != nullptr && m->settled && m->settledDate == (core::Date{2026, 9, 22});
        }
        check(settledNow, "al marcarlo pagado en Cotizaciones, el ingreso queda cobrado sin tocar nada");
        check(findRepair(qrepo.loadRepairs(), "asus x556U") != nullptr &&
                  findRepair(qrepo.loadRepairs(), "asus x556U")->status == core::RepairStatus::Cobrada,
              "y la reparacion, cobrada");

        // Anotar el cobro a mano seria cobrarlo dos veces: la captura no deja.
        const std::size_t count = qrepo.loadMovements().size();
        auto* input = quotesWindow.findChild<QLineEdit*>(QStringLiteral("CaptureInput"));
        if (input != nullptr) {
            // La reparacion ya esta cobrada y no se ofrece; se prueba con una
            // abierta: se reabre el informe como entregado.
            writeDoc(folder, QStringLiteral("INF-2026-005.json"),
                     informe("i5", "INF-2026-005", "entregado", "Luis", "RTX 3070", 5000, nullptr, false));
            emit quotesWindow.findChild<ui::SettingsPage*>()->quoteReadRequested();
            settle();
            const std::size_t withFive = qrepo.loadMovements().size();
            input->setFocus();
            QTest::keyClicks(input, QStringLiteral("50 cobro 3070"));
            QTest::keyClick(input, Qt::Key_Return);
            settle();
            check(withFive == count + 1, "el informe nuevo entro por cobrar");
            check(qrepo.loadMovements().size() == withFive,
                  "'50 cobro 3070' no crea un segundo ingreso: se cobra en Cotizaciones");
        }

        // La revision: Enter aplica lo sugerido (ignorar el repetido).
        onNextDialog([](QWidget* dialog) {
            QTest::keyClick(dialog, Qt::Key_Return);
        });
        emit quotesWindow.findChild<ui::SettingsPage*>()->quoteReviewRequested();
        reactivate(&quotesWindow);
        const QString decisions = qrepo.setting(QStringLiteral("cot.decisiones")).value_or(QString());
        check(decisions.contains(QStringLiteral("\"i2\":\"ignorar\"")),
              "revisar y Enter: el repetido queda ignorado");
        check(findMovement(qrepo.loadMovements(), "cot-i2-saldo") == nullptr, "y no entra");
    }

    // --- Revision de la semana -------------------------------------------------
    std::printf("\n[revision de la semana, con el teclado]\n");
    {
        const QString reviewPath = temp.path() + QStringLiteral("/revision.db");
        const QDate now = QDate::currentDate();
        const core::Date today = core::Date::fromYmd(now.year(), static_cast<unsigned>(now.month()),
                                                     static_cast<unsigned>(now.day()));
        {
            storage::Database setup(reviewPath);
            storage::Repository repo(setup);
            repo.seedIfEmpty(core::Currency::usd());
            repo.setSetting(QStringLiteral("cot.carpeta"), temp.path() + QStringLiteral("/sin-cotizaciones"));
            core::Recurring luz;
            luz.id = "R-luz";
            luz.name = "Luz";
            luz.category = "Luz";
            luz.pocketId = repo.loadPockets().front().id;
            luz.amountMinor = 40'00;
            luz.dayOfMonth = 1;
            luz.starts = today.firstDayOfMonth().addMonths(-2);
            repo.saveRecurring(luz);
            repo.saveCategory({"Herramientas", core::Account::Negocio, core::CategoryClass::Activo,
                               core::MovementKind::Gasto});
        }
        qputenv("DAKE_TEST_DB_PATH", reviewPath.toLocal8Bit());
        ui::MainWindow reviewWindow(reviewPath);
        reviewWindow.show();
        (void)QTest::qWaitForWindowExposed(&reviewWindow);
        reviewWindow.activateWindow();
        (void)QTest::qWaitForWindowActive(&reviewWindow);
        settle();

        storage::Database rdb(reviewPath);
        storage::Repository rrepo(rdb);
        int generated = 0;
        for (const core::MovementMeta& m : rrepo.loadMovementMeta()) {
            if (m.recurringId == "R-luz" && m.review == "confirmar") ++generated;
        }
        check(generated == 3, "la luz se anoto sola los tres meses, por confirmar");

        auto* input = reviewWindow.findChild<QLineEdit*>(QStringLiteral("CaptureInput"));
        if (input != nullptr) {
            input->setFocus();
            QTest::keyClicks(input, QStringLiteral("300 soldador herramientas"));
            QTest::keyClick(input, Qt::Key_Return);
            settle();
        }
        const auto tools = rrepo.loadTools();
        check(tools.size() == 1 && tools[0].costMinor == 300'00 && tools[0].lifeMonths == 24,
              "una compra de Herramientas se da de alta como herramienta, a 24 meses");

        // A la revision, con Ctrl+4.
        QTest::keyClick(&reviewWindow, Qt::Key_4, Qt::ControlModifier);
        settle();
        auto* review = reviewWindow.findChild<ui::ReviewPage*>();
        check(review != nullptr && review->isVisible(), "Ctrl+4 abre la revision");

        auto typeInFocus = [](const QString& text) {
            QWidget* focus = QApplication::focusWidget();
            if (focus == nullptr) {
                check(false, "sin foco para escribir " + text.toStdString());
                return;
            }
            if (auto* edit = qobject_cast<QLineEdit*>(focus)) edit->selectAll();
            if (!text.isEmpty()) QTest::keyClicks(focus, text);
            QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
            settle();
        };

        // El caso sembrado trae movimientos sin categoria; se resuelven todos
        // con la sugerencia o con "Varios". Despues la luz y la herramienta.
        int guard = 0;
        for (;;) {
            const auto metas = rrepo.loadMovementMeta();
            bool uncategorized = false;
            for (const core::Movement& m : rrepo.loadMovements()) {
                if (m.kind != core::MovementKind::Traspaso && m.category.empty()) uncategorized = true;
            }
            if (!uncategorized || ++guard > 20) break;
            typeInFocus(QStringLiteral("Varios"));
        }
        check(guard <= 20, "los movimientos sin categoria se resuelven escribiendola y Enter");

        typeInFocus(QStringLiteral("45"));  // primer mes: la factura vino por 45
        typeInFocus(QString());             // los otros dos: Enter
        typeInFocus(QString());
        int pendingLight = 0;
        std::int64_t firstAmount = 0;
        for (const core::MovementMeta& m : rrepo.loadMovementMeta()) {
            if (m.recurringId == "R-luz" && m.review == "confirmar") ++pendingLight;
        }
        for (const core::Movement& m : rrepo.loadMovements()) {
            if (m.id == "rec-R-luz-" + core::periodOf(today.firstDayOfMonth().addMonths(-2))) {
                firstAmount = m.amountMinor;
            }
        }
        check(pendingLight == 0, "la luz queda confirmada los tres meses");
        check(firstAmount == 45'00, "el primer mes, con el monto corregido");
        const auto recurring = rrepo.loadRecurring();
        check(!recurring.empty() && recurring[0].amountMinor == 45'00,
              "y 45 pasa a ser el estimado del mes que viene");

        typeInFocus(QStringLiteral("36"));  // vida util del soldador
        const auto after = rrepo.loadTools();
        check(!after.empty() && after[0].lifeMonths == 36, "la vida util se corrige a 36 meses");
        // Lo que queda (reparaciones del caso sembrado sin horas, un cobro
        // atrasado) se pospone una semana con Ctrl+P.
        for (int i = 0; i < 20 && rrepo.timingMedian(QStringLiteral("revision")) < 0; ++i) {
            QTest::keyClick(&reviewWindow, Qt::Key_P, Qt::ControlModifier);
            settle();
        }
        check(rrepo.setting(QStringLiteral("bandeja.pospuestos")).has_value(),
              "Ctrl+P pospone lo que no se quiere resolver hoy");
        check(rrepo.timingMedian(QStringLiteral("revision")) >= 0,
              "con la bandeja vacia, la revision quedo cronometrada");
    }

    // --- Extracto del banco --------------------------------------------------
    std::printf("\n[extracto del banco]\n");
    {
        const QString bankPath = temp.path() + QStringLiteral("/banco.db");
        core::Id pocketId;
        {
            storage::Database setup(bankPath);
            storage::Repository repo(setup);
            repo.seedIfEmpty(core::Currency::usd());
            repo.setSetting(QStringLiteral("cot.carpeta"), temp.path() + QStringLiteral("/sin-cotizaciones"));
            pocketId = repo.loadPockets().front().id;
            core::Movement aMano;
            aMano.id = "m-cobro-a-mano";
            aMano.date = core::Date{2026, 9, 3};
            aMano.name = "cobro gpu";
            aMano.kind = core::MovementKind::Ingreso;
            aMano.amountMinor = 120'00;
            aMano.pocketId = pocketId;
            aMano.category = "Reparacion";
            repo.save(aMano);
        }
        const QString csvPath = temp.path() + QStringLiteral("/extracto.csv");
        {
            QFile csv(csvPath);
            csv.open(QIODevice::WriteOnly);
            csv.write("Fecha;Descripcion;Monto\r\n"
                      "01/09/2026;CAFE LA ESQUINA;-2,50\r\n"
                      "02/09/2026;TRANSFERENCIA JUAN PEREZ;120,00\r\n"
                      "03/09/2026;UBER *TRIP;-8,40\r\n");
        }
        qputenv("DAKE_TEST_DB_PATH", bankPath.toLocal8Bit());
        ui::MainWindow bankWindow(bankPath);
        bankWindow.show();
        (void)QTest::qWaitForWindowExposed(&bankWindow);
        settle();

        storage::Database bdb(bankPath);
        storage::Repository brepo(bdb);
        const std::size_t before = brepo.loadMovements().size();

        onNextDialog([](QWidget* dialog) { QTest::keyClick(dialog, Qt::Key_Return); });
        bankWindow.importBankFile(csvPath);
        reactivate(&bankWindow);
        const auto after = brepo.loadMovements();
        check(after.size() == before + 2, "el cafe y el uber entran; la transferencia no");
        bool cafe = false;
        for (const core::Movement& m : after) {
            if (m.name == "CAFE LA ESQUINA" && m.amountMinor == 2'50 && m.kind == core::MovementKind::Gasto &&
                m.date == (core::Date{2026, 9, 1})) {
                cafe = true;
            }
        }
        check(cafe, "el cafe, como gasto de 2,50 el 1 de septiembre");
        bool linked = false;
        for (const core::MovementMeta& meta : brepo.loadMovementMeta()) {
            if (meta.movementId == "m-cobro-a-mano" && !meta.externalRef.empty()) linked = true;
        }
        check(linked, "la transferencia se enlazo al cobro que ya estaba anotado a mano");

        onNextDialog([](QWidget* dialog) { QTest::keyClick(dialog, Qt::Key_Return); });
        bankWindow.importBankFile(csvPath);
        reactivate(&bankWindow);
        check(brepo.loadMovements().size() == before + 2, "importar el mismo extracto otra vez no agrega nada");
    }

    std::printf("\n%s\n", gFailures == 0 ? "Todo pasa." : "HAY FALLAS.");
    std::fflush(stdout);
    return gFailures == 0 ? 0 : 1;
}
