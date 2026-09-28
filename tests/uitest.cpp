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
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QTest>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include "capturewindow.hpp"
#include "categorybox.hpp"
#include "dake/storage/database.hpp"
#include "dake/storage/repository.hpp"
#include "dialogs.hpp"
#include "entryform.hpp"
#include "pendinglist.hpp"
#include "mainwindow.hpp"
#include "cards.hpp"
#include "pages.hpp"
#include "theme.hpp"

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

    std::printf("\n[fuentes y logo empaquetados]\n");
    check(QFile::exists(QStringLiteral(":/dake/fuentes/Anton-Regular.ttf")), "Anton va dentro del ejecutable");
    check(QFile::exists(QStringLiteral(":/dake/logo/DAke.png")), "y el logo tambien");
    check(ui::theme::fontsLoaded(), "Inter y Anton quedaron registradas");
    check(ui::theme::bodyFont(10).family() == QStringLiteral("Inter"), "el texto sale en Inter");
    check(ui::theme::figureFont(21).family() == QStringLiteral("Anton"), "las cifras grandes en Anton");

    std::printf("\n[tema: un papel cambia de color con el tema]\n");
    {
        ui::theme::setTheme(ui::theme::Tema::Claro);
        QLabel probe;
        ui::theme::setLabelColor(&probe, ui::theme::kNegative);
        probe.ensurePolished();
        check(probe.property("tono").toString() == QStringLiteral("gasto"),
              "el monto de un gasto lleva el papel 'gasto'");
        check(probe.palette().color(probe.foregroundRole()) == QColor(QStringLiteral("#E8590C")),
              "en claro, naranja #E8590C");
        ui::theme::setTheme(ui::theme::Tema::Oscuro);
        probe.ensurePolished();
        check(probe.palette().color(probe.foregroundRole()) == QColor(QStringLiteral("#FF922B")),
              "en oscuro, el mismo papel da #FF922B sin volver a tocar la etiqueta");
        check(QColor(ui::theme::kNegative) == QColor(QStringLiteral("#FF922B")), "y pintar a mano tambien lo ve");
        ui::theme::setTheme(ui::theme::Tema::Claro);
    }

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
    {
        auto* logo = window.findChild<QLabel*>(QStringLiteral("SidebarLogo"));
        auto* brand = window.findChild<QLabel*>(QStringLiteral("SidebarBrand"));
        check(logo != nullptr && !logo->pixmap().isNull(), "el logo de DakeLabs esta en la barra lateral");
        check(brand != nullptr && brand->text().contains(QStringLiteral("LABS")) &&
                  brand->font().family() == QStringLiteral("Anton"),
              "con DAKELABS en Anton");
    }
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window);

    storage::Database db(path);
    storage::Repository repository(db);

    // --- Anotar con el formulario ---------------------------------------------
    std::printf("\n[anotar con el formulario]\n");
    {
        auto* amount = window.findChild<QLineEdit*>(QStringLiteral("EntryAmount"));
        auto* category = window.findChild<ui::CategoryBox*>(QStringLiteral("EntryCategory"));
        auto* date = window.findChild<QDateEdit*>(QStringLiteral("EntryDate"));
        auto* error = window.findChild<QLabel*>(QStringLiteral("EntryError"));
        auto* from = window.findChild<QComboBox*>(QStringLiteral("EntryPocket"));
        auto* to = window.findChild<QComboBox*>(QStringLiteral("EntryTarget"));
        check(amount && category && date && error && from && to, "el formulario esta en Hoy");
        check(window.findChild<QLineEdit*>(QStringLiteral("CaptureInput")) == nullptr,
              "y la linea de captura ya no existe");
        auto type = [](QWidget* w, const QString& text) {
            w->setFocus();
            QTest::keyClicks(w, text);
        };
        auto saved = [&repository](const std::function<bool(const core::Movement&)>& pred) {
            int n = 0;
            for (const core::Movement& m : repository.loadMovements()) {
                if (!m.deleted && pred(m)) ++n;
            }
            return n;
        };
        if (amount && category && date && error && from && to) {
            check(date->date() == QDate::currentDate(), "la fecha arranca en hoy");
            // 1. Un gasto con una categoria nueva.
            type(amount, QStringLiteral("25"));
            category->lineEdit()->clear();
            type(category->lineEdit(), QStringLiteral("Almuerzo taller"));
            // Si el completador quedo abierto, el primer Enter elige; el segundo guarda.
            if (category->lineEdit()->completer() != nullptr &&
                category->lineEdit()->completer()->popup()->isVisible()) {
                QTest::keyClick(category->lineEdit()->completer()->popup(), Qt::Key_Escape);
            }
            QTest::keyClick(category->lineEdit(), Qt::Key_Return);
            settle();
            check(saved([](const core::Movement& m) {
                      return m.kind == core::MovementKind::Gasto && m.amountMinor == 25'00 &&
                             m.category == "Almuerzo taller" && m.name == "Almuerzo taller" && m.settled &&
                             m.spreadMonths == 1 && m.jobId.empty();
                  }) == 1,
                  "25 + categoria nueva + Enter guarda un gasto de 25,00, pagado, con la categoria como nombre");
            check(amount->text().isEmpty() && amount->hasFocus(), "el monto queda vacio y con el foco");
            check(category->category() == QStringLiteral("Almuerzo taller"), "la categoria se conserva");
            check(category->findText(QStringLiteral("Almuerzo taller")) >= 0, "y ya esta en la lista");
            auto* done = window.findChild<QLabel*>(QStringLiteral("EntryDone"));
            check(done != nullptr && done->text().contains(QStringLiteral("25,00")),
                  "y dice que quedo anotado");

            // 1b. Con el completador abierto, Enter elige la sugerencia y no guarda.
            {
                const std::size_t count = repository.loadMovements().size();
                category->lineEdit()->clear();
                type(amount, QStringLiteral("7"));
                type(category->lineEdit(), QStringLiteral("taller"));
                settle();
                QCompleter* completer = category->lineEdit()->completer();
                const bool open = completer != nullptr && completer->popup()->isVisible();
                if (open) {
                    QTest::keyClick(category->lineEdit(), Qt::Key_Return);
                    settle();
                    check(repository.loadMovements().size() == count,
                          "Enter con la lista de sugerencias abierta no guarda a medias");
                    QTest::keyClick(completer->popup(), Qt::Key_Escape);
                } else {
                    std::printf("      (el completador no se abrio sin pantalla: no se prueba)\n");
                }
                category->lineEdit()->clear();
                amount->clear();
                settle();
            }

            // 2. Un ingreso con la fecha cambiada, sin tocar el mouse.
            QTest::keyClick(amount, Qt::Key_I, Qt::AltModifier);
            settle();
            category->lineEdit()->clear();
            type(amount, QStringLiteral("40,5"));
            type(category->lineEdit(), QStringLiteral("Venta"));
            const QDate threeDaysAgo = date->date().addDays(-3);
            date->setDate(threeDaysAgo);
            QTest::keyClick(amount, Qt::Key_Return);
            settle();
            const std::string iso = threeDaysAgo.toString(Qt::ISODate).toStdString();
            check(saved([&iso](const core::Movement& m) {
                      return m.kind == core::MovementKind::Ingreso && m.amountMinor == 40'50 &&
                             m.category == "Venta" && m.date.toIso() == iso;
                  }) == 1,
                  "Alt+I, 40,5 y la fecha de hace tres dias: un ingreso de 40,50 con esa fecha");

            // 3. Sin categoria no se guarda.
            const std::size_t before = repository.loadMovements().size();
            category->lineEdit()->clear();
            type(amount, QStringLiteral("5"));
            QTest::keyClick(amount, Qt::Key_Return);
            settle();
            check(repository.loadMovements().size() == before &&
                      error->text() == QStringLiteral("Falta la categoría"),
                  "sin categoria no se guarda, y se dice por que");

            // 4. Una fecha futura no se guarda.
            type(category->lineEdit(), QStringLiteral("Venta"));
            date->setMaximumDate(QDate(9999, 1, 1));  // la prueba fuerza el limite del campo
            date->setDate(QDate::currentDate().addDays(2));
            QTest::keyClick(amount, Qt::Key_Return);
            settle();
            check(repository.loadMovements().size() == before &&
                      error->text() == QStringLiteral("La fecha no puede ser futura"),
                  "una fecha futura no se guarda");
            date->setDate(QDate::currentDate());
            amount->clear();

            // 5. Un traspaso al mismo bolsillo no se guarda.
            QTest::keyClick(amount, Qt::Key_T, Qt::AltModifier);
            settle();
            check(!category->isVisibleTo(&window) && to->isVisibleTo(&window),
                  "en traspaso se piden De y A, no la categoria");
            to->setCurrentIndex(from->currentIndex());
            type(amount, QStringLiteral("10"));
            QTest::keyClick(amount, Qt::Key_Return);
            settle();
            check(repository.loadMovements().size() == before &&
                      error->text() == QStringLiteral("De y A tienen que ser distintos"),
                  "un traspaso al mismo bolsillo no se guarda");

            // 6. El sueldo: de la caja del negocio a lo personal.
            from->setCurrentIndex(from->findData(QStringLiteral("p-caja")));
            to->setCurrentIndex(to->findData(QStringLiteral("p-personal")));
            amount->clear();
            type(amount, QStringLiteral("300"));
            QTest::keyClick(amount, Qt::Key_Return);
            settle();
            check(saved([](const core::Movement& m) {
                      return m.kind == core::MovementKind::Traspaso && m.amountMinor == 300'00 &&
                             m.pocketId == "p-caja" && m.targetPocketId == "p-personal";
                  }) == 1,
                  "un traspaso de 300 de la caja a Mio: el sueldo");

            // 7. Un gasto personal pagado con la caja del negocio se parte en
            //    sueldo y gasto personal.
            emit window.findChild<ui::SettingsPage*>()->categoryChanged(
                {"Farmacia", core::Account::Personal, core::CategoryClass::General,
                 core::MovementKind::Gasto});
            settle();
            QTest::keyClick(amount, Qt::Key_G, Qt::AltModifier);
            settle();
            from->setCurrentIndex(from->findData(QStringLiteral("p-caja")));
            category->lineEdit()->clear();
            type(amount, QStringLiteral("12"));
            type(category->lineEdit(), QStringLiteral("Farmacia"));
            QTest::keyClick(amount, Qt::Key_Return);
            settle();
            check(saved([](const core::Movement& m) {
                      return m.kind == core::MovementKind::Gasto && m.amountMinor == 12'00 &&
                             m.category == "Farmacia" && m.pocketId == "p-personal";
                  }) == 1 &&
                      saved([](const core::Movement& m) {
                          return m.kind == core::MovementKind::Traspaso && m.amountMinor == 12'00 &&
                                 m.pocketId == "p-caja";
                      }) == 1,
                  "farmacia pagada con la caja: sueldo de 12 y gasto personal de 12");

            // 7b. El bolsillo por defecto es el ultimo usado para ese tipo, al
            //     toque: un gasto desde Mio, ida y vuelta a Ingreso, y vuelve Mio.
            QTest::keyClick(amount, Qt::Key_G, Qt::AltModifier);
            settle();
            from->setCurrentIndex(from->findData(QStringLiteral("p-personal")));
            category->lineEdit()->clear();
            type(amount, QStringLiteral("4"));
            type(category->lineEdit(), QStringLiteral("Farmacia"));
            QTest::keyClick(amount, Qt::Key_Return);
            settle();
            QTest::keyClick(amount, Qt::Key_I, Qt::AltModifier);
            settle();
            QTest::keyClick(amount, Qt::Key_G, Qt::AltModifier);
            settle();
            check(from->currentData().toString() == QStringLiteral("p-personal"),
                  "volver a Gasto propone el bolsillo del ultimo gasto");

            // 8. Un bolsillo archivado no se ofrece.
            bool archivedOffered = false;
            for (const core::Pocket& pk : repository.loadPockets()) {
                if (pk.archived && from->findData(QString::fromStdString(pk.id)) >= 0) {
                    archivedOffered = true;
                }
            }
            check(!archivedOffered, "ningun bolsillo archivado aparece en el formulario");
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

    // --- Una reparacion que se agrega ya cobrada -------------------------------
    std::printf("\n[agregar una reparacion ya cobrada]\n");
    {
        onNextDialog([](QWidget* dialog) {
            auto* charged = dialog->findChild<QCheckBox*>(QStringLiteral("AlreadyCharged"));
            check(charged != nullptr && !charged->isChecked(), "Nueva reparacion ofrece 'Ya esta cobrada', sin marcar");
            typeAndEnter(dialog, QStringLiteral("placa"));
            typeAndEnter(dialog, QStringLiteral("Marta"));
            QTest::keyClicks(dialog->focusWidget(), QStringLiteral("Asus B450"));
            // Sin pantalla el dialogo no queda como ventana activa y los atajos
            // Alt+letra no le llegan: se marca con Tab y espacio, que tambien
            // es teclado.
            QTest::keyClick(dialog->focusWidget(), Qt::Key_Tab);
            check(dialog->focusWidget() == charged, "Tab desde el equipo llega a 'Ya esta cobrada'");
            QTest::keyClick(dialog->focusWidget(), Qt::Key_Space);
            settle();
            check(charged != nullptr && charged->isChecked(), "espacio la marca");
            // Despues de crear se abre el de entregar y cobrar: horas y precio.
            onNextDialog([](QWidget* deliver) {
                typeTabThenEnter(deliver, QStringLiteral("1,5"), QStringLiteral("60"));
            });
            QTest::keyClick(dialog->focusWidget(), Qt::Key_Return);
        });
        QTest::keyClick(&window, Qt::Key_R, Qt::ControlModifier);
        reactivate(&window);
        reactivate(&window);

        const auto repairs = repository.loadRepairs();
        const core::Repair* placa = findRepair(repairs, "Asus B450");
        check(placa != nullptr && placa->status == core::RepairStatus::Cobrada &&
                  placa->realMinutes == 90,
              "queda cobrada, con 1,5 horas reales");
        bool income = false;
        for (const core::Movement& m : repository.loadMovements()) {
            if (placa != nullptr && m.jobId == placa->jobId && m.kind == core::MovementKind::Ingreso &&
                m.amountMinor == 60'00 && m.settled) {
                income = true;
            }
        }
        check(income, "y con el ingreso de 60,00 cobrado");
    }

    // --- La pastilla de estado ------------------------------------------------
    std::printf("\n[la ficha muestra el estado como pastilla]\n");
    {
        const auto repairs = repository.loadRepairs();
        const core::Repair* rtx = findRepair(repairs, "RTX 3080");
        if (rtx != nullptr) {
            QTest::keyClick(&window, Qt::Key_2, Qt::ControlModifier);
            settle();
            auto* repairsPage = window.findChild<ui::RepairsPage*>();
            repairsPage->selectRepair(rtx->jobId);
            settle();
            auto* pill = repairsPage->findChild<QLabel*>(QStringLiteral("RepairStatus"));
            check(pill != nullptr && pill->text() == QStringLiteral("Cobrada") &&
                      pill->property("pill").toString() == QStringLiteral("cobrada"),
                  "la ficha muestra la pastilla 'Cobrada'");
            QStringList fields;
            for (QLabel* l : repairsPage->findChildren<QLabel*>()) {
                if (l->isVisibleTo(repairsPage)) fields << l->text();
            }
            check(!fields.contains(QStringLiteral("PRECIO")) && !fields.contains(QStringLiteral("CONSUMIBLES")) &&
                      !fields.contains(QStringLiteral("ENVÍO")) && fields.contains(QStringLiteral("HORAS REALES")),
                  "la ficha ya no pide precio, consumibles ni envio; si las horas");
        }
    }

    // --- Informes por pregunta ------------------------------------------------
    std::printf("\n[informes por pregunta]\n");
    {
        QTest::keyClick(&window, Qt::Key_5, Qt::ControlModifier);
        settle();
        auto* reports = window.findChild<ui::ReportsPage*>();
        auto* tabs = reports != nullptr ? reports->findChild<QTabWidget*>() : nullptr;
        QStringList names;
        if (tabs != nullptr) {
            for (int i = 0; i < tabs->count(); ++i) names << tabs->tabText(i);
        }
        std::printf("      (pestañas: %s)\n", names.join(QStringLiteral(" | ")).toStdString().c_str());
        check(reports != nullptr && reports->isVisible() &&
                  names == QStringList({QStringLiteral("Resumen"), QStringLiteral("El mes"),
                                        QStringLiteral("Gastos"), QStringLiteral("Trabajos"),
                                        QStringLiteral("Sueldo")}),
              "Ctrl+5 abre Informes con cinco pestañas, una por pregunta");
        auto cardTitles = [](QWidget* root) {
            QStringList out;
            for (QLabel* l : root->findChildren<QLabel*>(QStringLiteral("CardTitle"))) out << l->text();
            return out;
        };
        if (tabs != nullptr && tabs->count() == 5) {
            const QStringList resumen = cardTitles(tabs->widget(0));
            check(resumen.contains(QStringLiteral("CON QUÉ SE PAGÓ ESTE MES")) &&
                      resumen.contains(QStringLiteral("LO QUE HAY QUE MIRAR")),
                  "Resumen: con que se pago el mes y lo que hay que mirar");
            const QStringList mes = cardTitles(tabs->widget(1));
            check(mes.contains(QStringLiteral("MES A MES")) &&
                      mes.contains(QStringLiteral("CAJA Y RESULTADO NO SON EL MISMO NÚMERO")) &&
                      mes.contains(QStringLiteral("RESULTADO POR MES")),
                  "El mes: resultado, mes a mes, y caja contra resultado");
            check(cardTitles(tabs->widget(2)).contains(QStringLiteral("COSTO DE ESTRUCTURA")),
                  "Gastos: el costo de estructura");
            check(cardTitles(tabs->widget(3)).contains(QStringLiteral("MARGEN POR TRABAJO")),
                  "Trabajos: el margen por trabajo");
            check(tabs->widget(3)->findChildren<ui::KpiCard*>().size() == 5 &&
                      tabs->widget(0)->findChildren<ui::KpiCard*>().size() == 3,
                  "las cifras de Hoy estan en Resumen (3) y en Trabajos (5)");
        }
        auto* today = window.findChild<ui::TodayPage*>();
        check(today != nullptr && today->findChildren<ui::KpiCard*>().isEmpty() &&
                  cardTitles(today) == QStringList({QStringLiteral("ANOTAR"), QStringLiteral("PENDIENTES")}),
              "Hoy solo tiene Anotar y Pendientes");
        auto* pocketsPage = window.findChild<ui::PocketsPage*>();
        check(pocketsPage != nullptr && !cardTitles(pocketsPage).contains(QStringLiteral("MES A MES")),
              "Mes a mes ya no esta en Bolsillos");
        auto* table = window.findChild<ui::MovementsPage*>()->findChild<QTableWidget*>();
        QStringList headers;
        for (int c = 0; c < table->columnCount(); ++c) headers << table->horizontalHeaderItem(c)->text();
        check(!headers.contains(QStringLiteral("Dura")) && !headers.contains(QStringLiteral("Trabajo")),
              "Movimientos sin Dura ni Trabajo");

        ui::PocketDialog dialog(core::Currency::usd());
        auto* kind = dialog.findChild<QComboBox*>();
        auto* note = dialog.findChild<QLabel*>(QStringLiteral("EmergencyNote"));
        const int emergencia = kind != nullptr ? kind->findData(static_cast<int>(core::PocketKind::Emergencia)) : -1;
        check(emergencia >= 0 && note != nullptr, "Nuevo bolsillo ofrece Emergencia");
        if (emergencia >= 0 && note != nullptr) {
            kind->setCurrentIndex(emergencia);
            check(note->isVisibleTo(&dialog) && note->text().contains(QStringLiteral("APK")),
                  "y avisa que primero va el APK nuevo del telefono");
            kind->setCurrentIndex(kind->findData(static_cast<int>(core::PocketKind::Ahorro)));
            check(!note->isVisibleTo(&dialog), "con otro tipo, el aviso no aparece");
        }
    }

    // --- Editar una celda se ve -------------------------------------------------
    std::printf("\n[editar una celda se ve]\n");
    {
        QTest::keyClick(&window, Qt::Key_6, Qt::ControlModifier);  // Ajustes
        settle();
        QTableWidget* editable = nullptr;
        for (auto* t : window.findChild<ui::SettingsPage*>()->findChildren<QTableWidget*>()) {
            if (t->rowCount() > 0 && t->editTriggers() != QAbstractItemView::NoEditTriggers &&
                t->item(0, 0) != nullptr && (t->item(0, 0)->flags() & Qt::ItemIsEditable)) {
                editable = t;
                break;
            }
        }
        check(editable != nullptr, "hay una tabla editable en Ajustes");
        if (editable != nullptr) {
            editable->setCurrentCell(0, 0);
            editable->editItem(editable->item(0, 0));
            settle();
            auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
            if (editor == nullptr) editor = editable->findChild<QLineEdit*>();
            check(editor != nullptr, "doble clic abre el campo dentro de la celda");
            if (editor != nullptr) {
                QStyleOptionFrame opt;
                opt.initFrom(editor);
                opt.rect = editor->rect();
                opt.lineWidth = editor->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &opt, editor);
                const QRect text = editor->style()->subElementRect(QStyle::SE_LineEditContents, &opt, editor);
                std::printf("      (campo %dx%d, texto %d de alto, letra %d)\n", editor->width(),
                            editor->height(), text.height(), editor->fontMetrics().height());
                check(text.height() >= editor->fontMetrics().height(),
                      "y el texto que se escribe entra y se ve");
                QTest::keyClick(editor, Qt::Key_Escape);
                settle();
            }
        }
        QTest::keyClick(&window, Qt::Key_1, Qt::ControlModifier);
        settle();
    }

    // --- Solo se rellena la pagina visible ---------------------------------------
    std::printf("\n[solo se rellena la pagina que se mira]\n");
    {
        QTest::keyClick(&window, Qt::Key_5, Qt::ControlModifier);  // Informes
        settle();
        auto* reports = window.findChild<ui::ReportsPage*>();
        auto valueOf = [](ui::KpiCard* card) {
            QStringList texts;
            for (QLabel* l : card->findChildren<QLabel*>()) texts << l->text();
            return texts.join(QLatin1Char('|'));
        };
        ui::KpiCard* cashKpi = nullptr;
        for (ui::KpiCard* card : reports->findChildren<ui::KpiCard*>()) {
            if (valueOf(card).startsWith(QStringLiteral("CAJA DEL NEGOCIO"))) cashKpi = card;
        }
        check(cashKpi != nullptr, "Informes tiene la caja del negocio");
        if (cashKpi == nullptr) return 1;
        const QString before = valueOf(cashKpi);
        QTest::keyClick(&window, Qt::Key_1, Qt::ControlModifier);  // Hoy
        settle();
        auto* amount = window.findChild<QLineEdit*>(QStringLiteral("EntryAmount"));
        auto* category = window.findChild<ui::CategoryBox*>(QStringLiteral("EntryCategory"));
        QTest::keyClick(amount, Qt::Key_G, Qt::AltModifier);
        settle();
        auto* from = window.findChild<QComboBox*>(QStringLiteral("EntryPocket"));
        from->setCurrentIndex(from->findData(QStringLiteral("p-caja")));
        amount->setFocus();
        QTest::keyClicks(amount, QStringLiteral("3"));
        category->setCategory(QStringLiteral("Almuerzo taller"));
        QTest::keyClick(amount, Qt::Key_Return);
        settle();
        check(valueOf(cashKpi) == before, "anotar en Hoy no rellena Informes mientras no se mira");
        QTest::keyClick(&window, Qt::Key_5, Qt::ControlModifier);
        settle();
        check(valueOf(cashKpi) != before, "al abrir Informes ya muestra la caja con el gasto nuevo");
        QTest::keyClick(&window, Qt::Key_1, Qt::ControlModifier);
        settle();
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

    // --- Pendientes en Hoy ------------------------------------------------------
    std::printf("\n[pendientes en Hoy]\n");
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
            repo.setSetting(QStringLiteral("recorte.inicio"),
                            QString::fromStdString(today.addDays(-40).toIso()));
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

        auto* toolAmount = reviewWindow.findChild<QLineEdit*>(QStringLiteral("EntryAmount"));
        auto* toolCategory = reviewWindow.findChild<ui::CategoryBox*>(QStringLiteral("EntryCategory"));
        if (toolAmount != nullptr && toolCategory != nullptr) {
            toolAmount->setFocus();
            QTest::keyClicks(toolAmount, QStringLiteral("300"));
            toolCategory->lineEdit()->clear();
            toolCategory->setCategory(QStringLiteral("Herramientas"));
            QTest::keyClick(toolAmount, Qt::Key_Return);
            settle();
        }
        const auto tools = rrepo.loadTools();
        check(tools.size() == 1 && tools[0].costMinor == 300'00 && tools[0].lifeMonths == 24,
              "una compra de Herramientas se da de alta como herramienta, a 24 meses");

        // --- Hoy: los pendientes, todos a la vista --------------------------
        QList<QPushButton*> nav;
        for (QPushButton* b : reviewWindow.findChildren<QPushButton*>(QStringLiteral("NavButton"))) {
            if (!b->isCheckable()) continue;  // Conectar y Sincronizar no son secciones
            nav << b;
        }
        bool reviewInNav = false;
        for (QPushButton* b : nav) reviewInNav = reviewInNav || b->text().startsWith(QStringLiteral("Revisi"));
        {
            QStringList navTexts;
            for (QPushButton* b : nav) navTexts << b->text();
            std::printf("      (barra: %s)\n", navTexts.join(QStringLiteral(" | ")).toStdString().c_str());
        }
        check(nav.size() == 6 && !reviewInNav, "la barra tiene seis secciones y ya no esta Revision");
        check(reviewWindow.findChild<ui::PendingList*>() != nullptr, "los pendientes estan en Hoy");

        auto rows = [&reviewWindow](const QString& kind) {
            QList<QWidget*> out;
            for (QWidget* w : reviewWindow.findChildren<QWidget*>(QStringLiteral("Pending:") + kind)) {
                if (w->isVisibleTo(&reviewWindow)) out << w;
            }
            return out;
        };
        const int pocketsOpen = static_cast<int>(rrepo.loadPockets().size());
        check(rows(QStringLiteral("PorConfirmar")).size() == 3,
              "los tres meses de luz estan a la vista a la vez, no de a uno");
        check(rows(QStringLiteral("SinCuadrar")).size() == pocketsOpen,
              "y cada bolsillo sin cuadrar hace mas de 30 dias");

        // Confirmar el primero con otro monto, desde su fila.
        if (!rows(QStringLiteral("PorConfirmar")).isEmpty()) {
            auto* value = rows(QStringLiteral("PorConfirmar")).front()->findChild<QLineEdit*>(QStringLiteral("PendingValue"));
            check(value != nullptr, "la fila trae el monto para corregir");
            if (value != nullptr) {
                value->setFocus();
                value->selectAll();
                QTest::keyClicks(value, QStringLiteral("45"));
                QTest::keyClick(value, Qt::Key_Return);
                settle();
            }
        }
        int pendingLight = 0;
        int at45 = 0;
        for (const core::MovementMeta& m : rrepo.loadMovementMeta()) {
            if (m.recurringId == "R-luz" && m.review == "confirmar") ++pendingLight;
        }
        for (const core::Movement& m : rrepo.loadMovements()) {
            if (m.name == "Luz" && m.amountMinor == 45'00) ++at45;
        }
        check(pendingLight == 2 && at45 == 1, "un mes de luz queda confirmado en 45,00; los otros dos siguen");
        const auto recurring = rrepo.loadRecurring();
        check(!recurring.empty() && recurring[0].amountMinor == 45'00,
              "y 45 pasa a ser el estimado del mes que viene");
        check(rows(QStringLiteral("PorConfirmar")).size() == 2 &&
                  rows(QStringLiteral("SinCuadrar")).size() == pocketsOpen,
              "la fila resuelta desaparece y las demas siguen en su lugar");

        // Cuadrar un bolsillo desde su fila: el dialogo de siempre, Enter.
        if (!rows(QStringLiteral("SinCuadrar")).isEmpty()) {
            onNextDialog([](QWidget* dialog) { QTest::keyClick(dialog, Qt::Key_Return); });
            auto* action = rows(QStringLiteral("SinCuadrar")).front()->findChild<QPushButton*>(QStringLiteral("PendingAction"));
            check(action != nullptr && action->text() == QStringLiteral("Cuadrar"), "la fila dice Cuadrar");
            if (action != nullptr) QTest::mouseClick(action, Qt::LeftButton);
            reactivate(&reviewWindow);
        }
        bool stamped = false;
        for (const core::Pocket& pk : rrepo.loadPockets()) {
            stamped = stamped || rrepo.setting(QStringLiteral("bolsillo.%1.cuadrado")
                                                   .arg(QString::fromStdString(pk.id)))
                                     .has_value();
        }
        check(stamped && rows(QStringLiteral("SinCuadrar")).size() == pocketsOpen - 1,
              "cuadrado aunque no haya diferencia: queda la fecha y la fila se va");

        // Despues: una semana.
        if (!rows(QStringLiteral("SinCuadrar")).isEmpty()) {
            auto* later = rows(QStringLiteral("SinCuadrar")).front()->findChild<QPushButton*>(QStringLiteral("PendingLater"));
            if (later != nullptr) QTest::mouseClick(later, Qt::LeftButton);
            settle();
        }
        check(rows(QStringLiteral("SinCuadrar")).size() == pocketsOpen - 2 &&
                  rrepo.setting(QStringLiteral("bandeja.pospuestos")).value_or(QString()).contains(QStringLiteral(":cuadrar")),
              "Despues pospone esa fila una semana");
    }

    std::printf("\n[la ventana chica usa el mismo formulario]\n");
    {
        ui::CaptureWindow mini;
        check(mini.findChild<ui::EntryForm*>() != nullptr && mini.entry() != nullptr,
              "Ctrl+Alt+Espacio abre el mismo formulario");
    }

    std::printf("\n[tema oscuro desde Ajustes, y que se recuerde]\n");
    {
        check(ui::theme::temaFromString(QStringLiteral("azul")) == ui::theme::Tema::Claro,
              "un valor raro arranca en claro");
        check(ui::theme::temaFromString(QString()) == ui::theme::Tema::Claro, "sin valor, claro");
        check(ui::theme::temaFromString(QStringLiteral(" Oscuro ")) == ui::theme::Tema::Oscuro,
              "'Oscuro' con espacios, oscuro");

        auto* selector = window.findChild<QComboBox*>(QStringLiteral("TemaSelector"));
        auto* amount = window.findChild<QLineEdit*>(QStringLiteral("EntryAmount"));
        check(selector != nullptr && amount != nullptr, "el selector de tema esta en Ajustes");
        if (selector != nullptr && amount != nullptr) {
            QTest::keyClick(amount, Qt::Key_G, Qt::AltModifier);
            settle();
            check(amount->property("tono").toString() == QStringLiteral("gasto"),
                  "el monto de un gasto lleva el papel 'gasto'");
            selector->setCurrentIndex(1);
            settle();
            check(ui::theme::currentTheme() == ui::theme::Tema::Oscuro, "elegir Oscuro cambia el tema");
            check(amount->palette().color(QPalette::Text) == QColor(QStringLiteral("#FF922B")),
                  "y el monto ya esta en el naranja del oscuro, sin reabrir nada");
            check(repository.setting(QStringLiteral("ui.tema")).value_or(QString()) == QStringLiteral("oscuro"),
                  "queda guardado en ui.tema");
        }
    }
    {
        ui::theme::setTheme(ui::theme::Tema::Claro);
        ui::MainWindow again(path);
        check(ui::theme::currentTheme() == ui::theme::Tema::Oscuro, "al reabrir, arranca en oscuro");
    }
    ui::theme::setTheme(ui::theme::Tema::Claro);

    std::printf("\n%s\n", gFailures == 0 ? "Todo pasa." : "HAY FALLAS.");
    std::fflush(stdout);
    return gFailures == 0 ? 0 : 1;
}
