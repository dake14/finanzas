#include "mainwindow.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QButtonGroup>
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSqlDatabase>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QLineEdit>

#include <algorithm>

#include "dake/core/accounts.hpp"
#include "dake/core/hlc.hpp"
#include "dake/storage/quoterows.hpp"
#include "dake/sync/config.hpp"
#include "dialogs.hpp"
#include "quotedialog.hpp"
#include "repairdialogs.hpp"
#include "pages.hpp"
#include "capturewindow.hpp"
#include "entryform.hpp"
#include "pendinglist.hpp"
#include "globalhotkey.hpp"
#include "theme.hpp"

namespace dake::ui {
namespace {

/// Reloj logico compartido por toda la sesion. Existe desde el dia uno aunque
/// no haya servidor: retro-adaptar identificadores y relojes sobre una base con
/// datos reales es doloroso, y agregar el transporte encima es trivial.
core::HlcClock& clock(const std::string& deviceId) {
    static core::HlcClock instance(deviceId);
    return instance;
}

/// Las categorias de lo que nace de una reparacion. Si no existen, se crean
/// del negocio la primera vez que se usan.
const std::string kRepairIncomeCategory = "Reparaciones";
const std::string kRepairPartsCategory = "Repuestos";

/// Cuanto se espera despues del ultimo cambio antes de subir. Cinco segundos
/// alcanzan para agrupar una tanda de anotaciones seguidas y son poco para
/// quien anota una sola cosa y cierra la aplicacion.
constexpr int kAutoSyncDelayMs = 5000;

/// Cada cuanto se mira el servidor sin que pase nada aca. Diez minutos: lo que
/// se anota en el telefono no es urgente —nadie mira las dos pantallas a la
/// vez— y una consulta cada diez minutos no se nota ni en la red ni en la
/// cuota del proyecto.
constexpr int kPeriodicSyncMs = 10 * 60 * 1000;

[[nodiscard]] QPushButton* navButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFont(theme::bodyFont(10, QFont::DemiBold));
    button->setFixedHeight(38);
    button->setObjectName(QStringLiteral("NavButton"));
    return button;
}

} // namespace

MainWindow::MainWindow(const QString& dbPath, QWidget* parent) : QMainWindow(parent) {
    db_ = std::make_unique<storage::Database>(dbPath);
    repository_ = std::make_unique<storage::Repository>(*db_);
    if (!theme::fontsLoaded()) {
        // La app sigue con Segoe UI; se ve distinta pero anda. Que quede escrito.
        qWarning("Finanzas: no se pudieron registrar Inter/Anton desde el recurso; se usa Segoe UI.");
    }
    // El tema va antes que cualquier widget: asi nada se construye con la
    // hoja de otro tema.
    theme::setTheme(theme::temaFromString(
        repository_->setting(QStringLiteral("ui.tema")).value_or(QString())));
    deviceId_ = storage::deviceId(*repository_);

    snapshot_.currency = core::Currency::usd();
    const QDate now = QDate::currentDate();
    snapshot_.today = core::Date::fromYmd(now.year(), static_cast<unsigned>(now.month()),
                                          static_cast<unsigned>(now.day()));

    // Solo corre sobre una base sin un solo movimiento, o sea en la primera
    // apertura. Sobre datos existentes no hace nada, asi que no puede pisar lo
    // anotado ni cuando se sincroniza.
    repository_->seedIfEmpty(snapshot_.currency);

    supabase_ = std::make_unique<sync::SupabaseClient>(sync::SupabaseConfig::load(), this);
    syncEngine_ = std::make_unique<sync::SyncEngine>(*supabase_, *repository_, sync::desktopTables(), this);

    connect(supabase_.get(), &sync::SupabaseClient::signedIn, this, [this](const QString& email) {
        repository_->setSetting(QStringLiteral("sync.refresh_token"), supabase_->refreshToken());
        repository_->setSetting(QStringLiteral("sync.user_email"), email);
        updateCloudUi(email);
        syncEngine_->sync();
    });
    connect(supabase_.get(), &sync::SupabaseClient::signedOut, this, [this]() {
        updateCloudUi(QStringLiteral("Sin sesión"));
    });
    connect(supabase_.get(), &sync::SupabaseClient::authFailed, this, [this](const QString& message) {
        // Un fallo de sesion durante una sincronizacion automatica no
        // interrumpe: quien lo provoco fue un temporizador, no el usuario.
        // Queda escrito en el estado de la nube, que es donde va a mirar
        // cuando note que hace rato no sube nada.
        if (lastSyncWasAutomatic_) {
            updateCloudUi(message);
            return;
        }
        QMessageBox::warning(this, QStringLiteral("Error"), message);
    });

    connect(syncEngine_.get(), &sync::SyncEngine::progress, this, [this](const QString& text, int, int) {
        updateCloudUi(text);
    });
    connect(syncEngine_.get(), &sync::SyncEngine::finished, this, [this](int uploaded, int downloaded, const QString& error) {
        if (error.isEmpty()) {
            updateCloudUi(QStringLiteral("%1 ↑, %2 ↓").arg(uploaded).arg(downloaded));
            reload();
            // Lo que bajo de Cotizaciones entra en la misma corrida.
            if (downloaded > 0) importQuotes(true);
        } else {
            updateCloudUi(error);
        }
    });

    autoSyncTimer_ = new QTimer(this);
    autoSyncTimer_->setSingleShot(true);
    autoSyncTimer_->setInterval(kAutoSyncDelayMs);
    connect(autoSyncTimer_, &QTimer::timeout, this, &MainWindow::runAutoSync);

    periodicSyncTimer_ = new QTimer(this);
    periodicSyncTimer_->setInterval(kPeriodicSyncMs);
    connect(periodicSyncTimer_, &QTimer::timeout, this, &MainWindow::runAutoSync);
    periodicSyncTimer_->start();

    buildUi();
    buildTray();

    // El atajo elegido en Ajustes, y si otro programa ya lo tiene, el primero
    // libre de una lista corta. Ctrl+Alt+Espacio es el preferido pero hay
    // programas que lo toman; Ctrl+Alt+N es el Ctrl+N de adentro de la
    // aplicacion, que ya significa "anotar". Lo elegido no se pisa: si
    // mañana el otro programa lo suelta, vuelve a usarse.
    QStringList candidates;
    if (const auto stored = repository_->setting(QStringLiteral("config.atajo"))) {
        candidates << *stored;
    }
    candidates << QStringLiteral("Ctrl+Alt+Space") << QStringLiteral("Ctrl+Alt+N")
               << QStringLiteral("Ctrl+Shift+Space");
    candidates.removeDuplicates();
    const bool testRun = !qEnvironmentVariableIsEmpty("DAKE_TEST_DB_PATH");
    for (const QString& candidate : candidates) {
        if (testRun || applyHotkey(QKeySequence(candidate, QKeySequence::PortableText))) {
            break;
        }
    }
    // Por defecto arranca con Windows: el atajo no sirve si la aplicacion no
    // esta abierta. Se reescribe en cada arranque para que siga la ruta del
    // ejecutable si se movio de carpeta.
    applyAutostart(repository_->setting(QStringLiteral("config.arranque")).value_or(QStringLiteral("1")) ==
                   QStringLiteral("1"));

    reload();
    importQuotes(false);
    generateRecurring();

    // Cada hora: si cambio el dia con la aplicacion abierta, puede haber
    // recurrentes nuevos.
    hourlyTimer_ = new QTimer(this);
    hourlyTimer_->setInterval(60 * 60 * 1000);
    connect(hourlyTimer_, &QTimer::timeout, this, [this] {
        const QDate now = QDate::currentDate();
        snapshot_.today = core::Date::fromYmd(now.year(), static_cast<unsigned>(now.month()),
                                              static_cast<unsigned>(now.day()));
        generateRecurring();
    });
    hourlyTimer_->start();

    const auto token = repository_->setting(QStringLiteral("sync.refresh_token"));
    if (token) {
        supabase_->restoreSession(*token);
    }
}

MainWindow::~MainWindow() {
    // La ventana mini no tiene padre (es de nivel superior) y hay que
    // borrarla a mano.
    delete captureWindow_;
}

// ----------------------------------------------------------- Bandeja y atajo

namespace {

/// El icono de la bandeja: una "F" sobre el rojo de la marca. Pintado y no
/// cargado de un archivo, para no depender de recursos.
[[nodiscard]] QIcon trayIcon() {
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(theme::color(theme::kAccent));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRectF(2, 2, 60, 60), 14, 14);
    painter.setPen(Qt::white);
    painter.setFont(theme::displayFont(30, QFont::Bold));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("F"));
    return QIcon(pixmap);
}

} // namespace

void MainWindow::buildTray() {
    setWindowIcon(trayIcon());
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    tray_ = new QSystemTrayIcon(trayIcon(), this);
    tray_->setToolTip(QStringLiteral("Finanzas DakeLabs"));

    auto* menu = new QMenu(this);
    menu->addAction(QStringLiteral("Anotar"), this, &MainWindow::showCapture);
    menu->addAction(QStringLiteral("Abrir Finanzas"), this, &MainWindow::showMainWindow);
    menu->addSeparator();
    menu->addAction(QStringLiteral("Salir"), this, [this] {
        quitting_ = true;
        QCoreApplication::quit();
    });
    tray_->setContextMenu(menu);
    connect(tray_, &QSystemTrayIcon::messageClicked, this, [this] {
        showMainWindow();
        if (!snapshot_.inbox.empty()) showPage(3);
    });

    connect(tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick) {
                    showMainWindow();
                }
            });
    tray_->show();
}

bool MainWindow::applyHotkey(const QKeySequence& sequence) {
    const bool ok = hotkey_->setShortcut(sequence);
    if (tray_ != nullptr) {
        tray_->setToolTip(ok ? QStringLiteral("Finanzas DakeLabs · %1 para anotar")
                                   .arg(sequence.toString(QKeySequence::NativeText))
                             : QStringLiteral("Finanzas DakeLabs"));
    }
    return ok;
}

void MainWindow::applyAutostart(bool enabled) {
#ifdef Q_OS_WIN
    // Una corrida sobre una base de prueba no toca el arranque de Windows: la
    // entrada apuntaria a un ejecutable de prueba.
    if (!qEnvironmentVariableIsEmpty("DAKE_TEST_DB_PATH")) {
        return;
    }
    QSettings run(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    const QString name = QCoreApplication::applicationName();
    if (enabled) {
        run.setValue(name, QStringLiteral("\"%1\" --bandeja")
                               .arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    } else {
        run.remove(name);
    }
#else
    Q_UNUSED(enabled);
#endif
}

void MainWindow::showCapture() {
    captureWindow_->popup();
}

void MainWindow::showMainWindow() {
    if (isMinimized()) {
        showNormal();
    } else {
        show();
    }
    raise();
    activateWindow();
}

void MainWindow::handleInstanceMessage(const QString& message) {
    if (message == QStringLiteral("anotar")) {
        showCapture();
    } else {
        showMainWindow();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (quitting_ || tray_ == nullptr) {
        QMainWindow::closeEvent(event);
        QCoreApplication::quit();
        return;
    }
    hide();
    event->ignore();
    // Se avisa una sola vez. La primera vez que la ventana desaparece hay que
    // decir donde quedo; la decima, es ruido.
    if (!repository_->setting(QStringLiteral("config.aviso_bandeja"))) {
        repository_->setSetting(QStringLiteral("config.aviso_bandeja"), QStringLiteral("1"));
        tray_->showMessage(QStringLiteral("Finanzas sigue abierta"),
                           QStringLiteral("Quedó en la bandeja del sistema. %1 para anotar desde "
                                          "cualquier programa; clic derecho en el ícono para salir.")
                               .arg(hotkey_->shortcut().toString(QKeySequence::NativeText)),
                           QSystemTrayIcon::Information, 6000);
    }
}

void MainWindow::restamp(std::string& hlc, std::string& deviceId) {
    hlc = clock(deviceId_.toStdString()).now(QDateTime::currentMSecsSinceEpoch()).encode();
    deviceId = deviceId_.toStdString();
}

core::Id MainWindow::stamp(std::string& hlc, std::string& deviceId) {
    restamp(hlc, deviceId);
    return storage::newId();
}

void MainWindow::rememberUndo(const std::optional<core::Movement>& previo,
                              const core::Movement& despues, const QString& que) {
    undoAlso_.clear();
    undoBefore_ = previo;
    undoAfter_ = despues;
    undoLabel_ = que;
}

// --------------------------------------------------------------------- UI

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("Finanzas DakeLabs"));
    resize(1240, 860);

    auto* central = new QWidget(this);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sidebar = new QWidget(central);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(216);
    buildSidebar(sidebar);
    root->addWidget(sidebar);

    stack_ = new QStackedWidget(central);
    today_ = new TodayPage(stack_);
    repairs_ = new RepairsPage(stack_);
    movements_ = new MovementsPage(stack_);
    pockets_ = new PocketsPage(stack_);
    reports_ = new ReportsPage(stack_);
    settings_ = new SettingsPage(stack_);
    // El orden importa: es el mismo que el de los botones de la barra y el que
    // usa showPage().
    for (QWidget* page : {static_cast<QWidget*>(today_), static_cast<QWidget*>(repairs_),
                          static_cast<QWidget*>(movements_),
                          static_cast<QWidget*>(pockets_), static_cast<QWidget*>(reports_),
                          static_cast<QWidget*>(settings_)}) {
        stack_->addWidget(page);
    }
    root->addWidget(stack_, 1);
    setCentralWidget(central);

    // Los dos formularios —el de Hoy y el de la ventana chica— guardan igual.
    // Lo unico distinto es que la chica se esconde al guardar, salvo con
    // Shift+Enter.
    captureWindow_ = new CaptureWindow();
    const auto wire = [this](EntryForm* form, bool mini) {
        connect(form, &EntryForm::submitted, this,
                [this, form, mini](const core::Movement& m, const core::Category& c, bool keepOpen) {
                    // Antes de guardar: guardar recarga, y la recarga tiene que
                    // ver ya el bolsillo recien usado para ese tipo.
                    rememberPockets(m);
                    if (!addMovement(m, c)) {
                        return;
                    }
                    // CONTRATO (unidad U-C, parte 4): summary es
                    // savedSummary(m) seguido de saveNote_.
                    const QString summary = savedSummary(m) + saveNote_;
                    form->confirmSaved(summary);
                    if (mini && !keepOpen) {
                        captureWindow_->hide();
                        if (tray_ != nullptr && !isVisible()) {
                            tray_->showMessage(QStringLiteral("Anotado"), summary,
                                               QSystemTrayIcon::Information, 2500);
                        }
                    }
                });
    };
    wire(today_->entry(), false);
    wire(captureWindow_->entry(), true);
    connect(captureWindow_->entry(), &EntryForm::cancelled, captureWindow_, &QWidget::hide);

    hotkey_ = new GlobalHotkey(this);
    connect(hotkey_, &GlobalHotkey::activated, this, &MainWindow::showCapture);
    connect(settings_, &SettingsPage::themeChanged, this, [this](theme::Tema tema) {
        theme::setTheme(tema);
        repository_->setSetting(QStringLiteral("ui.tema"), theme::toString(tema));
        // Las tablas y el texto enriquecido guardan el color con que se
        // llenaron: se vuelven a llenar. Lo pintado a mano, solo se repinta.
        reload();
        for (QWidget* widget : QApplication::allWidgets()) {
            widget->update();
        }
    });
    connect(settings_, &SettingsPage::hotkeyChanged, this, [this](const QKeySequence& sequence) {
        if (applyHotkey(sequence)) {
            repository_->setSetting(QStringLiteral("config.atajo"),
                                    sequence.toString(QKeySequence::PortableText));
        }
        reload();
    }, Qt::QueuedConnection);
    connect(settings_, &SettingsPage::autostartChanged, this, [this](bool enabled) {
        repository_->setSetting(QStringLiteral("config.arranque"),
                                enabled ? QStringLiteral("1") : QStringLiteral("0"));
        applyAutostart(enabled);
        reload();
    }, Qt::QueuedConnection);
    // Todo lo que llega de la ficha va encolado: la recarga que sigue
    // reconstruye la ficha, y reconstruirla dentro de la senal de uno de sus
    // propios campos cuelga la aplicacion.
    connect(repairs_, &RepairsPage::newRepairRequested, this, &MainWindow::newRepair);
    connect(repairs_, &RepairsPage::repairEdited, this, &MainWindow::editRepair, Qt::QueuedConnection);
    connect(repairs_, &RepairsPage::deliverRequested, this, &MainWindow::deliverRepair, Qt::QueuedConnection);
    connect(repairs_, &RepairsPage::chargeRequested, this, &MainWindow::chargeRepair, Qt::QueuedConnection);
    connect(repairs_, &RepairsPage::partAdded, this, &MainWindow::addPart, Qt::QueuedConnection);
    connect(repairs_, &RepairsPage::partChanged, this, &MainWindow::changePart, Qt::QueuedConnection);
    connect(repairs_, &RepairsPage::partRemoved, this, &MainWindow::removePart, Qt::QueuedConnection);

    // Los pendientes de Hoy: todo encolado, porque cada accion recarga la
    // lista que la emitio.
    PendingList* pending = today_->pending();
    connect(pending, &PendingList::recurringConfirmed, this, &MainWindow::confirmRecurring, Qt::QueuedConnection);
    connect(pending, &PendingList::quotesReviewRequested, this, &MainWindow::reviewQuotes, Qt::QueuedConnection);
    connect(pending, &PendingList::chargeRequested, this, &MainWindow::chargeRepair, Qt::QueuedConnection);
    connect(pending, &PendingList::repairOpened, this,
            [this](const core::Id& jobId) {
                showPage(1);
                repairs_->selectRepair(jobId);
            },
            Qt::QueuedConnection);
    connect(pending, &PendingList::realHoursSet, this, &MainWindow::setRealHours, Qt::QueuedConnection);
    connect(pending, &PendingList::partCostSet, this, &MainWindow::setPartCost, Qt::QueuedConnection);
    connect(pending, &PendingList::reconcileRequested, this, &MainWindow::reconcile, Qt::QueuedConnection);
    connect(pending, &PendingList::snoozed, this, &MainWindow::snooze, Qt::QueuedConnection);

    auto* repairShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
    connect(repairShortcut, &QShortcut::activated, this, &MainWindow::newRepair);
    connect(movements_, &MovementsPage::movementActivated, this, &MainWindow::editMovement);
    connect(pockets_, &PocketsPage::newPocketRequested, this, &MainWindow::newPocket);
    connect(pockets_, &PocketsPage::reconcileRequested, this, &MainWindow::reconcile);
    // Encolados: los dos vienen de un boton o un desplegable que la recarga
    // posterior destruye, y destruirlo dentro de su propia senal cuelga la app.
    connect(pockets_, &PocketsPage::accountToggled, this, &MainWindow::togglePocketAccount,
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::categoryChanged, this, &MainWindow::saveCategory,
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::costSettingsChanged, this,
            [this](const core::CostSettings& settings) {
                repository_->saveCostSettings(settings);
                reload();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::templateChanged, this,
            [this](const core::RepairTemplate& tpl) {
                repository_->saveTemplate(tpl);
                reload();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::templateAdded, this,
            [this] {
                core::RepairTemplate tpl;
                tpl.id = storage::newId();
                tpl.name = "Nueva plantilla";
                repository_->saveTemplate(tpl);
                reload();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::quoteReviewRequested, this, &MainWindow::reviewQuotes,
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::recurringChanged, this,
            [this](const core::Recurring& r) {
                repository_->saveRecurring(r);
                reload();
                generateRecurring();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::recurringAdded, this,
            [this] {
                // Empieza este mes y en cero: no genera nada hasta tener monto.
                core::Recurring r;
                r.id = storage::newId();
                r.name = "Nuevo gasto fijo";
                r.category = r.name;
                r.pocketId = businessPocket();
                r.dayOfMonth = 1;
                r.starts = snapshot_.today.firstDayOfMonth();
                repository_->saveRecurring(r);
                reload();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::recurringRemoved, this,
            [this](const core::Id& id) {
                repository_->removeRecurring(id);
                reload();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::toolChanged, this,
            [this](const core::Tool& t) {
                repository_->saveTool(t);
                reload();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::toolAdded, this,
            [this] {
                core::Tool t;
                t.id = storage::newId();
                t.name = "Nueva herramienta";
                t.bought = snapshot_.today;
                repository_->saveTool(t);
                reload();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::toolRemoved, this,
            [this](const core::Id& id) {
                repository_->removeTool(id);
                reload();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::fallbackHoursChanged, this,
            [this](int minutes) {
                repository_->setSetting(QStringLiteral("config.horas_mes"), QString::number(minutes));
                reload();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::splitChanged, this,
            [this](const core::ProfitSplit& split) {
                repository_->setSetting(QStringLiteral("config.reparto.sueldo"), QString::number(split.salaryBps));
                repository_->setSetting(QStringLiteral("config.reparto.impuestos"), QString::number(split.taxesBps));
                repository_->setSetting(QStringLiteral("config.reparto.reinversion"), QString::number(split.reinvestBps));
                repository_->setSetting(QStringLiteral("config.reparto.emergencia"), QString::number(split.emergencyBps));
                reload();
            },
            Qt::QueuedConnection);
    // CONTRATO (unidad U-C, parte 2): conectar SettingsPage::personalSavingsChanged
    // igual que splitChanged de arriba (Qt::QueuedConnection): guardar el bps
    // en el setting "config.ahorro_personal" con QString::number y llamar a
    // reload().
    connect(settings_, &SettingsPage::personalSavingsChanged, this,
            [this](int bps) {
                repository_->setSetting(QStringLiteral("config.ahorro_personal"), QString::number(bps));
                reload();
            },
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::templateRemoved, this,
            [this](const core::Id& id) {
                repository_->removeTemplate(id);
                reload();
            },
            Qt::QueuedConnection);

    // Ctrl+1 a Ctrl+6: cada seccion a una tecla, para no tocar el mouse.
    for (int index = 0; index < 6; ++index) {
        auto* go = new QShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + index)),
                                 this);
        connect(go, &QShortcut::activated, this, [this, index] { showPage(index); });
    }

    // Ctrl+N va a anotar y Ctrl+F a buscar. Son los dos verbos de la
    // aplicacion; el resto se puede alcanzar con el mouse sin que duela.
    auto* newShortcut = new QShortcut(QKeySequence::New, this);
    connect(newShortcut, &QShortcut::activated, this, [this] {
        showPage(0);
        today_->entry()->focusAmount();
    });
    auto* findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this] {
        showPage(2);
        movements_->focusSearch();
    });

    auto* syncShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this);
    connect(syncShortcut, &QShortcut::activated, this, &MainWindow::syncNow);

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::undoLast);

    showPage(0);
}

void MainWindow::buildSidebar(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(16, 22, 16, 18);
    layout->setSpacing(6);

    auto* brandRow = new QHBoxLayout();
    brandRow->setSpacing(8);
    auto* logo = new QLabel(parent);
    logo->setObjectName(QStringLiteral("SidebarLogo"));
    logo->setPixmap(QPixmap(QStringLiteral(":/dake/logo/DAke.png"))
                        .scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brandRow->addWidget(logo);
    // La barra es azul-noche en los dos temas: DAKE siempre blanco, LABS en la
    // marca. Por eso el color va escrito aca y no por papel.
    auto* brand = new QLabel(QStringLiteral("<span style='color:#FFFFFF'>DAKE</span>"
                                            "<span style='color:#EF233C'>LABS</span>"),
                             parent);
    brand->setObjectName(QStringLiteral("SidebarBrand"));
    brand->setFont(theme::figureFont(18));
    brandRow->addWidget(brand);
    brandRow->addStretch(1);
    layout->addLayout(brandRow);

    auto* subtitle = new QLabel(QStringLiteral("Finanzas"), parent);
    subtitle->setFont(theme::bodyFont(9));
    layout->addWidget(subtitle);
    layout->addSpacing(18);

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    const QStringList names{QStringLiteral("Hoy"), QStringLiteral("Reparaciones"),
                            QStringLiteral("Movimientos"), QStringLiteral("Bolsillos"),
                            QStringLiteral("Informes"), QStringLiteral("Ajustes")};
    for (int index = 0; index < names.size(); ++index) {
        QPushButton* button = navButton(names[index], parent);
        group->addButton(button);
        navButtons_.append(button);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, index] { showPage(index); });
    }

    layout->addStretch(1);

    cloudButton_ = navButton(QStringLiteral("Conectar"), parent);
    cloudButton_->setCheckable(false);
    connect(cloudButton_, &QPushButton::clicked, this, &MainWindow::toggleSignIn);
    layout->addWidget(cloudButton_);

    syncButton_ = navButton(QStringLiteral("Sincronizar"), parent);
    syncButton_->setCheckable(false);
    syncButton_->setEnabled(false);
    connect(syncButton_, &QPushButton::clicked, this, &MainWindow::syncNow);
    layout->addWidget(syncButton_);

    cloudStatus_ = new QLabel(parent);
    cloudStatus_->setFont(theme::bodyFont(8));
    cloudStatus_->setWordWrap(true);
    theme::setLabelColor(cloudStatus_, theme::kTextFaint);
    layout->addWidget(cloudStatus_);

    layout->addSpacing(18);

    auto* reset = new QPushButton(QStringLiteral("Borrar todo"), parent);
    reset->setCursor(Qt::PointingHandCursor);
    reset->setFont(theme::bodyFont(9));
    reset->setToolTip(QStringLiteral("Borra tus bolsillos, trabajos y movimientos, "
                                     "acá y en el teléfono."));
    connect(reset, &QPushButton::clicked, this, &MainWindow::deleteEverything);
    layout->addWidget(reset);

    footer_ = new QLabel(parent);
    footer_->setFont(theme::bodyFont(8));
    footer_->setWordWrap(true);
    theme::setLabelColor(footer_, theme::kTextFaint);
    layout->addWidget(footer_);
}

void MainWindow::showPage(int index) {
    stack_->setCurrentIndex(index);
    refreshPage(index);
    for (int i = 0; i < navButtons_.size(); ++i) {
        navButtons_[i]->setChecked(i == index);
    }
}

void MainWindow::refreshPage(int index) {
    if (index < 0 || index >= kPages || !stale_[static_cast<std::size_t>(index)]) {
        return;
    }
    stale_[static_cast<std::size_t>(index)] = false;
    QElapsedTimer clock;
    clock.start();
    switch (index) {
        case 0: today_->setSnapshot(snapshot_); break;
        case 1: repairs_->setSnapshot(snapshot_); break;
        case 2: movements_->setSnapshot(snapshot_); break;
        case 3: pockets_->setSnapshot(snapshot_); break;
        case 4: reports_->setSnapshot(snapshot_); break;
        case 5: settings_->setSnapshot(snapshot_); break;
        default: break;
    }
    pageMs_[static_cast<std::size_t>(index)] = clock.elapsed();
}

QString MainWindow::measure() {
    QElapsedTimer clock;
    clock.start();
    reload();
    const qint64 total = clock.elapsed();
    QStringList pages;
    const QStringList names{QStringLiteral("hoy"), QStringLiteral("reparaciones"), QStringLiteral("movimientos"),
                            QStringLiteral("bolsillos"), QStringLiteral("informes"), QStringLiteral("ajustes")};
    for (int i = 0; i < kPages; ++i) {
        stale_[static_cast<std::size_t>(i)] = true;
        refreshPage(i);
        pages << QStringLiteral("%1 %2").arg(names[i]).arg(pageMs_[static_cast<std::size_t>(i)]);
    }
    return QStringLiteral("recarga %1 ms (datos y calculos %2, pagina visible %3) · cada pagina: %4 ms")
        .arg(total)
        .arg(dataMs_)
        .arg(total - dataMs_)
        .arg(pages.join(QStringLiteral(", ")));
}

void MainWindow::reload() {
    QElapsedTimer clock;
    clock.start();
    snapshot_.pockets = repository_->loadPockets();
    snapshot_.jobs = repository_->loadJobs();
    snapshot_.movements = repository_->loadMovements();

    // Las categorias que aparecen en los movimientos y no estan en la tabla
    // —las de antes de la version 3, las que llegan del telefono— se adoptan
    // con la cuenta deducida de donde se pagaron. Nadie tiene que
    // configurarlas a mano para que los reportes separen negocio y personal.
    snapshot_.categories = repository_->loadCategories();
    const auto adopted =
        core::inferCategories(snapshot_.movements, snapshot_.pockets, snapshot_.categories);
    if (!adopted.empty()) {
        for (const core::Category& category : adopted) {
            repository_->saveCategory(category);
        }
        snapshot_.categories = repository_->loadCategories();
    }

    // Las reparaciones, con el estado que dicen sus movimientos: anotar
    // "120 cobro GPU 3080" en la captura la deja cobrada igual que el boton.
    // Lo que cambia se guarda, para que el telefono vea el trabajo cerrado.
    snapshot_.repairs = repository_->loadRepairs();
    bool reconciledAny = false;
    for (core::Repair& repair : snapshot_.repairs) {
        const core::Repair reconciled = core::reconcileRepair(repair, snapshot_.movements);
        if (reconciled.status != repair.status || reconciled.delivered != repair.delivered) {
            repair = reconciled;
            persistRepair(repair);
            reconciledAny = true;
        }
    }
    if (reconciledAny && supabase_->isSignedIn()) {
        autoSyncTimer_->start();
    }
    snapshot_.parts = repository_->loadRepairParts();
    snapshot_.templates = repository_->loadTemplates();
    snapshot_.costs = repository_->loadCostSettings();
    snapshot_.jobs = repository_->loadJobs();

    snapshot_.recurring = repository_->loadRecurring();
    snapshot_.tools = repository_->loadTools();
    snapshot_.metas = repository_->loadMovementMeta();
    snapshot_.fallbackMinutesPerMonth =
        repository_->setting(QStringLiteral("config.horas_mes")).value_or(QStringLiteral("4800")).toInt();
    snapshot_.fixedRate = core::fixedRate(snapshot_.recurring, snapshot_.tools, snapshot_.repairs,
                                          snapshot_.today, snapshot_.fallbackMinutesPerMonth,
                                          snapshot_.currency);
    snapshot_.costs.fixedPerHourMinor = snapshot_.fixedRate.perHour.minor();
    {
        core::ProfitSplit split;
        split.salaryBps = repository_->setting(QStringLiteral("config.reparto.sueldo")).value_or(QStringLiteral("5500")).toInt();
        split.taxesBps = repository_->setting(QStringLiteral("config.reparto.impuestos")).value_or(QStringLiteral("1500")).toInt();
        split.reinvestBps = repository_->setting(QStringLiteral("config.reparto.reinversion")).value_or(QStringLiteral("2000")).toInt();
        split.emergencyBps = repository_->setting(QStringLiteral("config.reparto.emergencia")).value_or(QStringLiteral("1000")).toInt();
        snapshot_.split = split.valid() ? split : core::ProfitSplit{};
    }
    // CONTRATO (unidad U-C, parte 1): snapshot_.personalSavingsBps = el
    // setting "config.ahorro_personal" como entero; si falta, no es un numero
    // o queda fuera de 0 a 10000, core::kDefaultPersonalSavingsBps.
    bool ok = false;
    int val = 0;
    if (auto s = repository_->setting(QStringLiteral("config.ahorro_personal"))) {
        val = s->toInt(&ok);
    }
    snapshot_.personalSavingsBps = (ok && val >= 0 && val <= 10000) ? val : core::kDefaultPersonalSavingsBps;
    snapshot_.salary = core::salaryAdvice(snapshot_.movements, snapshot_.pockets, snapshot_.categories,
                                          snapshot_.tools, snapshot_.split, snapshot_.today,
                                          snapshot_.currency);

    core::InboxInput inboxInput;
    inboxInput.movements = snapshot_.movements;
    inboxInput.metas = snapshot_.metas;
    inboxInput.repairs = snapshot_.repairs;
    inboxInput.parts = snapshot_.parts;
    inboxInput.quoteHolds = snapshot_.quoteHolds();
    inboxInput.pockets = snapshot_.pockets;
    {
        // Un bolsillo que nunca se cuadro cuenta desde el dia en que esta
        // version corrio por primera vez: si no, aparecerian todos juntos.
        const QString since = repository_->setting(QStringLiteral("recorte.inicio")).value_or(QString());
        try {
            inboxInput.reconcileSince = core::Date::fromIso(since.toStdString());
        } catch (const std::exception&) {
            inboxInput.reconcileSince = snapshot_.today;
            repository_->setSetting(QStringLiteral("recorte.inicio"),
                                    QString::fromStdString(snapshot_.today.toIso()));
        }
        for (const core::Pocket& p : snapshot_.pockets) {
            const auto stamp = repository_->setting(
                QStringLiteral("bolsillo.%1.cuadrado").arg(QString::fromStdString(p.id)));
            if (!stamp) continue;
            try {
                inboxInput.lastReconciled.emplace_back(p.id, core::Date::fromIso(stamp->toStdString()));
            } catch (const std::exception&) {
                // Una fecha rota en los ajustes no puede tumbar la recarga.
            }
        }
    }
    inboxInput.today = snapshot_.today;
    {
        const QString raw = repository_->setting(QStringLiteral("bandeja.pospuestos")).value_or(QString());
        const QJsonObject object = QJsonDocument::fromJson(raw.toUtf8()).object();
        for (auto it = object.begin(); it != object.end(); ++it) {
            try {
                inboxInput.snoozed.emplace_back(it.key().toStdString(),
                                                core::Date::fromIso(it.value().toString().toStdString()));
            } catch (const std::exception&) {
                // Una fecha rota en los ajustes no puede tumbar la recarga.
            }
        }
    }
    snapshot_.inbox = core::inbox(inboxInput);
    snapshot_.lastExpensePocket = repository_->setting(QStringLiteral("anotar.ultimo.gasto")).value_or(QString());
    snapshot_.lastIncomePocket = repository_->setting(QStringLiteral("anotar.ultimo.ingreso")).value_or(QString());
    snapshot_.lastTransferFrom = repository_->setting(QStringLiteral("anotar.ultimo.traspaso.de")).value_or(QString());
    snapshot_.lastTransferTo = repository_->setting(QStringLiteral("anotar.ultimo.traspaso.a")).value_or(QString());

    snapshot_.hotkey = hotkey_ != nullptr ? hotkey_->shortcut().toString(QKeySequence::PortableText)
                                          : QString();
    snapshot_.hotkeyRegistered = hotkey_ != nullptr && hotkey_->isRegistered();
    snapshot_.autostart = repository_->setting(QStringLiteral("config.arranque"))
                              .value_or(QStringLiteral("1")) == QStringLiteral("1");
    dataMs_ = clock.elapsed();

    // Solo se rellena la pagina que se esta mirando; las demas, al mostrarse.
    // Rellenar las seis despues de cada cambio (Informes sobre todo) era lo
    // que hacia lenta la ficha de una reparacion.
    stale_.fill(true);
    refreshPage(stack_->currentIndex());
    const int pending = static_cast<int>(snapshot_.inbox.size());
    navButtons_[0]->setText(pending > 0 ? QStringLiteral("Hoy (%1)").arg(pending) : QStringLiteral("Hoy"));
    if (tray_ != nullptr) {
        tray_->setToolTip(QStringLiteral("Finanzas DakeLabs · %1 pendientes").arg(pending));
    }
    if (captureWindow_ != nullptr) {
        captureWindow_->entry()->setSnapshot(snapshot_);
    }

    footer_->setText(db_->path());
}

// ---------------------------------------------------------------- Acciones

bool MainWindow::addMovement(const core::Movement& draft, const core::Category& newCategory) {
    saveNote_.clear();
    const core::Account account = !newCategory.name.empty()
                                      ? newCategory.account
                                      : snapshot_.categoryAccount(draft.category, draft.pocketId);

    // Un gasto personal pagado con plata del negocio se guarda como lo que es:
    // un sueldo y un gasto personal. El negocio no registra un almuerzo.
    std::vector<core::Movement> parts =
        core::splitCrossExpense(draft, snapshot_.pockets, account, snapshot_.personalPocket());
    // CONTRATO (unidad U-C, parte 3):
    //  a) saveNote_.clear() al entrar a esta funcion (antes de todo lo demas).
    //  b) Si parts tiene un solo elemento: savings = core::personalSavingsPocket(
    //     snapshot_.pockets); parts = core::splitPersonalIncome(draft, account,
    //     savings, snapshot_.personalSavingsBps).
    //  c) Si ese paso partio en dos: saveNote_ = " · " + <monto de parts[1]
    //     con theme::formatMoney(core::Money::fromMinor(..., snapshot_.currency))>
    //     + " a " + snapshot_.pocketName(savings).
    //     Si no partio, pero draft es Ingreso, draft.settled, account es
    //     Personal, snapshot_.personalSavingsBps > 0 y savings esta vacio:
    //     saveNote_ = " · Crea un bolsillo de Ahorro personal para apartar el "
    //     + QString::number(snapshot_.personalSavingsBps / 100.0, 'g', 4) con
    //     el punto cambiado por coma + " %".
    //  d) Mas abajo, `main` (lo que va a rememberUndo) deja de ser parts.back():
    //     es el primer elemento de parts cuyo kind es draft.kind, y undoAlso_
    //     son todos los demas elementos de parts, en su orden. La compra de
    //     herramienta sigue usando parts.back() (ese caso es siempre un Gasto).
    //  Si el guardado falla, saveNote_ puede quedar con cualquier valor.
    if (parts.size() == 1) {
        core::Id savings = core::personalSavingsPocket(snapshot_.pockets);
        parts = core::splitPersonalIncome(draft, account, savings, snapshot_.personalSavingsBps);
        if (parts.size() == 2) {
            saveNote_ = QStringLiteral(" · %1 a %2")
                            .arg(theme::formatMoney(core::Money::fromMinor(parts[1].amountMinor, snapshot_.currency)),
                                 snapshot_.pocketName(savings));
        } else if (draft.kind == core::MovementKind::Ingreso && draft.settled &&
                   account == core::Account::Personal && snapshot_.personalSavingsBps > 0 &&
                   savings.empty()) {
            saveNote_ = QStringLiteral(" · Crea un bolsillo de Ahorro personal para apartar el %1 %")
                            .arg(QString::number(snapshot_.personalSavingsBps / 100.0, 'g', 4).replace(QLatin1Char('.'), QLatin1Char(',')));
        }
    }
    for (core::Movement& part : parts) {
        part.id = stamp(part.hlc, part.deviceId);
    }

    const bool ownTx = db_->handle().transaction();
    try {
        if (!newCategory.name.empty()) {
            repository_->saveCategory(newCategory);
        } else if (!draft.category.empty() &&
                   core::findCategory(snapshot_.categories, draft.category) == nullptr) {
            repository_->saveCategory({draft.category, account, core::CategoryClass::General,
                                       draft.kind});
        }
        for (const core::Movement& part : parts) {
            repository_->save(part);
        }
        // Una compra de herramienta no es un gasto del mes: se deprecia. Se
        // da de alta con 24 meses de vida; si son otros, se cambia en Ajustes.
        const core::Category* category = core::findCategory(snapshot_.categories, draft.category);
        const bool isTool = (category != nullptr && category->cls == core::CategoryClass::Activo);
        if (isTool && draft.kind == core::MovementKind::Gasto) {
            const core::Movement& purchase = parts.back();
            core::Tool tool;
            tool.id = storage::newId();
            tool.name = purchase.name;
            tool.costMinor = purchase.amountMinor;
            tool.bought = purchase.date;
            tool.lifeMonths = 24;
            tool.movementId = purchase.id;
            repository_->saveTool(tool);
        }
        if (ownTx && !db_->handle().commit()) {
            throw storage::StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
        }
    } catch (const std::exception& error) {
        if (ownTx) db_->handle().rollback();
        QMessageBox::critical(this, QStringLiteral("No se pudo guardar"),
                              QString::fromUtf8(error.what()));
        return false;
    }

    auto mainIt = std::find_if(parts.begin(), parts.end(),
                               [&draft](const core::Movement& m) { return m.kind == draft.kind; });
    const core::Movement& main = (mainIt != parts.end()) ? *mainIt : parts.front();
    rememberUndo(std::nullopt, main,
                 QStringLiteral("anotar «%1»").arg(QString::fromStdString(main.name)));
    for (auto it = parts.begin(); it != parts.end(); ++it) {
        if (it != mainIt) {
            undoAlso_.push_back(*it);
        }
    }
    afterLocalChange();
    return true;
}

void MainWindow::rememberPockets(const core::Movement& movement) {
    const QString from = QString::fromStdString(movement.pocketId);
    switch (movement.kind) {
        case core::MovementKind::Gasto:
            repository_->setSetting(QStringLiteral("anotar.ultimo.gasto"), from);
            break;
        case core::MovementKind::Ingreso:
            repository_->setSetting(QStringLiteral("anotar.ultimo.ingreso"), from);
            break;
        case core::MovementKind::Traspaso:
            repository_->setSetting(QStringLiteral("anotar.ultimo.traspaso.de"), from);
            repository_->setSetting(QStringLiteral("anotar.ultimo.traspaso.a"),
                                    QString::fromStdString(movement.targetPocketId));
            break;
    }
    snapshot_.lastExpensePocket = repository_->setting(QStringLiteral("anotar.ultimo.gasto")).value_or(QString());
    snapshot_.lastIncomePocket = repository_->setting(QStringLiteral("anotar.ultimo.ingreso")).value_or(QString());
    snapshot_.lastTransferFrom = repository_->setting(QStringLiteral("anotar.ultimo.traspaso.de")).value_or(QString());
    snapshot_.lastTransferTo = repository_->setting(QStringLiteral("anotar.ultimo.traspaso.a")).value_or(QString());
}

QString MainWindow::savedSummary(const core::Movement& movement) const {
    const QString money =
        theme::formatMoney(core::Money::fromMinor(movement.amountMinor, snapshot_.currency));
    switch (movement.kind) {
        case core::MovementKind::Gasto:
            return QStringLiteral("Gasto %1 · %2 · %3")
                .arg(money, QString::fromStdString(movement.category), snapshot_.pocketName(movement.pocketId));
        case core::MovementKind::Ingreso:
            return QStringLiteral("Ingreso %1 · %2 · %3")
                .arg(money, QString::fromStdString(movement.category), snapshot_.pocketName(movement.pocketId));
        case core::MovementKind::Traspaso:
            return QStringLiteral("%1 %2 · %3 → %4")
                .arg(QString::fromStdString(movement.name), money, snapshot_.pocketName(movement.pocketId),
                     snapshot_.pocketName(movement.targetPocketId));
    }
    return money;
}

void MainWindow::togglePocketAccount(const core::Id& pocketId) {
    const core::Pocket* pocket = snapshot_.pocket(pocketId);
    if (pocket == nullptr) {
        return;
    }
    const core::Account next = core::accountOf(*pocket) == core::Account::Personal
                                   ? core::Account::Negocio
                                   : core::Account::Personal;
    // Si la cuenta nueva es la que ya le toca por tipo, se borra la marca en
    // vez de guardarla: asi un cambio de tipo posterior sigue mandando.
    core::Pocket plain = *pocket;
    plain.accountOverride.reset();
    repository_->setPocketAccount(pocketId, core::accountOf(plain) == next
                                                ? std::nullopt
                                                : std::optional<core::Account>(next));
    reload();
}

void MainWindow::saveCategory(const core::Category& category) {
    repository_->saveCategory(category);
    reload();
}

void MainWindow::newPocket() {
    PocketDialog dialog(snapshot_.currency, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto pocket = dialog.result();
    if (!pocket) {
        return;
    }
    core::Pocket saved = *pocket;
    saved.id = stamp(saved.hlc, saved.deviceId);
    repository_->save(saved);
    afterLocalChange();
}

// ------------------------------------------------------- Fijos y bandeja

void MainWindow::clearReview(const core::Id& movementId) {
    if (const core::MovementMeta* meta = snapshot_.meta(movementId)) {
        core::MovementMeta cleared = *meta;
        cleared.review.clear();
        repository_->saveMovementMeta(cleared);
    }
}

void MainWindow::generateRecurring() {
    const auto due = core::dueRecurring(snapshot_.recurring, snapshot_.metas, snapshot_.today);
    if (due.empty()) {
        return;
    }
    const bool ownTx = db_->handle().transaction();
    try {
        for (const core::GeneratedRecurring& g : due) {
            // La categoria de un recurrente es un gasto fijo del negocio: es
            // lo que forma la tasa de fijos por hora.
            if (!g.movement.category.empty() &&
                core::findCategory(snapshot_.categories, g.movement.category) == nullptr) {
                repository_->saveCategory({g.movement.category, core::Account::Negocio,
                                           core::CategoryClass::Fija, core::MovementKind::Gasto});
                snapshot_.categories = repository_->loadCategories();
            }
            persistGenerated(g.movement);
            repository_->saveMovementMeta(g.meta);
        }
        if (ownTx && !db_->handle().commit()) {
            throw storage::StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
        }
    } catch (const std::exception& error) {
        if (ownTx) db_->handle().rollback();
        updateCloudUi(QStringLiteral("Recurrentes: ") + QString::fromUtf8(error.what()));
        return;
    }
    afterLocalChange();
}

void MainWindow::confirmRecurring(const core::Id& movementId, qint64 amountMinor) {
    const core::Movement* movement = snapshot_.movement(movementId);
    const core::MovementMeta* meta = snapshot_.meta(movementId);
    if (movement == nullptr) return;
    if (movement->amountMinor != amountMinor) {
        core::Movement updated = *movement;
        updated.amountMinor = amountMinor;
        restamp(updated.hlc, updated.deviceId);
        repository_->save(updated);
        // La factura vino distinta: ese es el estimado del mes que viene.
        if (meta != nullptr) {
            for (core::Recurring r : snapshot_.recurring) {
                if (r.id == meta->recurringId) {
                    r.amountMinor = amountMinor;
                    repository_->saveRecurring(r);
                }
            }
        }
    }
    clearReview(movementId);
    afterLocalChange();
}

void MainWindow::setRealHours(const core::Id& jobId, int minutes) {
    const core::Repair* repair = snapshot_.repair(jobId);
    if (repair == nullptr) return;
    core::Repair updated = *repair;
    updated.realMinutes = minutes;
    repository_->saveRepair(updated);
    afterLocalChange();
}

void MainWindow::setPartCost(const core::Id& partId, qint64 costMinor) {
    const core::RepairPart* part = snapshot_.part(partId);
    if (part == nullptr) return;
    core::RepairPart updated = *part;
    updated.costMinor = costMinor;
    updated.costKnown = true;
    changePart(updated);
}

void MainWindow::snooze(const std::string& id, int days) {
    const QString raw = repository_->setting(QStringLiteral("bandeja.pospuestos")).value_or(QString());
    QJsonObject object = QJsonDocument::fromJson(raw.toUtf8()).object();
    // Lo vencido se limpia de paso: la lista no crece para siempre.
    const QString today = QString::fromStdString(snapshot_.today.toIso());
    for (const QString& key : object.keys()) {
        if (object.value(key).toString() <= today) object.remove(key);
    }
    object.insert(QString::fromStdString(id), QString::fromStdString(snapshot_.today.addDays(days).toIso()));
    repository_->setSetting(QStringLiteral("bandeja.pospuestos"),
                            QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    reload();
}

// ------------------------------------------------------ DakeLabs Cotizaciones

namespace {

[[nodiscard]] bool sameRepair(const core::Repair& a, const core::Repair& b) {
    return a.jobId == b.jobId && a.orderNo == b.orderNo && a.device == b.device &&
           a.type == b.type && a.templateId == b.templateId && a.received == b.received &&
           a.delivered == b.delivered && a.status == b.status && a.priceMinor == b.priceMinor &&
           a.shippingMinor == b.shippingMinor && a.consumablesMinor == b.consumablesMinor &&
           a.estMinutes == b.estMinutes && a.realMinutes == b.realMinutes &&
           a.sourceRef == b.sourceRef;
}

[[nodiscard]] bool sameIncome(const core::Movement& a, const core::Movement& b) {
    return a.amountMinor == b.amountMinor && a.date == b.date && a.settled == b.settled &&
           a.settledDate == b.settledDate && a.jobId == b.jobId && a.kind == b.kind;
}

} // namespace

core::QuoteDecisions MainWindow::loadQuoteDecisions() {
    core::QuoteDecisions out;
    const QString raw = repository_->setting(QStringLiteral("cot.decisiones")).value_or(QString());
    const QJsonObject object = QJsonDocument::fromJson(raw.toUtf8()).object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        out[it.key().toStdString()] = it.value().toString().toStdString();
    }
    return out;
}

void MainWindow::saveQuoteDecisions(const core::QuoteDecisions& decisions) {
    QJsonObject object;
    for (const auto& [id, decision] : decisions) {
        object.insert(QString::fromStdString(id), QString::fromStdString(decision));
    }
    repository_->setSetting(QStringLiteral("cot.decisiones"),
                            QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
}

bool MainWindow::applyQuotePlan(const core::QuotePlan& plan,
                                const std::set<std::string>& tombstones) {
    if (plan.decision != core::QuoteDecision::Importar || !plan.repair) {
        return false;
    }
    bool changed = false;
    const core::Repair& repair = *plan.repair;
    const bool closed = repair.status == core::RepairStatus::Cobrada;
    const std::string name = repair.orderNo + " · " + repair.device;

    // El trabajo, que es lo que sincroniza.
    const core::Job* job = snapshot_.job(repair.jobId);
    if (job == nullptr) {
        core::Job fresh;
        fresh.id = repair.jobId;
        restamp(fresh.hlc, fresh.deviceId);
        fresh.name = name;
        fresh.client = plan.client;
        fresh.opened = repair.received.value_or(snapshot_.today);
        fresh.closed = closed;
        repository_->save(fresh);
        snapshot_.jobs.push_back(fresh);
        changed = true;
    } else if (job->client != plan.client || job->name != name || job->closed != closed) {
        core::Job updated = *job;
        updated.client = plan.client;
        updated.name = name;
        updated.closed = closed;
        restamp(updated.hlc, updated.deviceId);
        repository_->save(updated);
        changed = true;
    }

    const core::Repair* current = snapshot_.repair(repair.jobId);
    if (current == nullptr || !sameRepair(*current, repair)) {
        repository_->saveRepair(repair);
        changed = true;
    }

    for (const std::string& partName : plan.newParts) {
        core::RepairPart part;
        part.id = storage::newId();
        part.jobId = repair.jobId;
        part.name = partName;
        part.costKnown = false;
        repository_->saveRepairPart(part);
        changed = true;
    }

    for (const core::Movement& income : plan.incomes) {
        // Un ingreso importado que se borro a mano no vuelve: alguien decidio
        // que sobraba, y pelearle esa decision cada vez que se lee la carpeta
        // es peor que el problema.
        if (tombstones.contains(income.id)) {
            continue;
        }
        const auto it = std::find_if(snapshot_.movements.begin(), snapshot_.movements.end(),
                                     [&income](const core::Movement& m) { return m.id == income.id; });
        if (it == snapshot_.movements.end() || !sameIncome(*it, income)) {
            persistGenerated(income);
            changed = true;
        }
    }
    return changed;
}

void MainWindow::importQuotes(bool notify) {
    const storage::QuoteRowsRead read = storage::readQuoteRows(*repository_);
    snapshot_.quoteErrors = read.errors;
    snapshot_.quoteDocs = read.docs;

    core::QuoteContext context;
    context.repairs = snapshot_.repairs;
    context.jobs = snapshot_.jobs;
    context.movements = snapshot_.movements;
    context.parts = snapshot_.parts;
    context.decisions = loadQuoteDecisions();
    context.pocketId = businessPocket();
    context.category = kRepairIncomeCategory;
    snapshot_.quotePlans = core::planQuotes(read.docs, context);

    std::set<std::string> tombstones;
    for (const core::Movement& m : repository_->loadMovements(true)) {
        if (m.deleted) tombstones.insert(m.id);
    }

    QStringList arrived;
    const bool ownTx = db_->handle().transaction();
    try {
        for (const core::QuotePlan& plan : snapshot_.quotePlans) {
            if (applyQuotePlan(plan, tombstones)) {
                const core::QuoteDoc* doc = snapshot_.quoteDoc(plan.docId);
                arrived << QString::fromStdString(plan.number) +
                               (doc != nullptr && doc->status == "pagado" ? QStringLiteral(" cobrado")
                                                                          : QString());
            }
        }
        if (ownTx && !db_->handle().commit()) {
            throw storage::StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
        }
    } catch (const std::exception& error) {
        if (ownTx) db_->handle().rollback();
        updateCloudUi(QStringLiteral("Cotizaciones: ") + QString::fromUtf8(error.what()));
        return;
    }

    if (!arrived.isEmpty()) {
        if (notify && tray_ != nullptr) {
            tray_->showMessage(QStringLiteral("Desde Cotizaciones"), arrived.join(QStringLiteral(", ")),
                               QSystemTrayIcon::Information, 4000);
        }
        afterLocalChange();
    } else {
        // Nada cambio en la base, pero los documentos y lo que espera si:
        // Ajustes lo muestra.
        settings_->setSnapshot(snapshot_);
    }
    const int holds = snapshot_.quoteHolds();
    if (notify && holds > 0 && tray_ != nullptr && arrived.isEmpty()) {
        tray_->showMessage(QStringLiteral("Cotizaciones"),
                           QStringLiteral("%1 documento(s) esperan que decidas qué hacer con ellos: "
                                          "Ajustes → Revisar.").arg(holds),
                           QSystemTrayIcon::Information, 4000);
    }
}

void MainWindow::reviewQuotes() {
    QuoteReviewDialog dialog(snapshot_, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    core::QuoteDecisions decisions = loadQuoteDecisions();
    for (const auto& [id, decision] : dialog.decisions()) {
        decisions[id] = decision;
    }
    saveQuoteDecisions(decisions);
    importQuotes(false);
}

// ----------------------------------------------------------- Reparaciones

core::Id MainWindow::businessPocket() const {
    return core::suggestedPocket(snapshot_.pockets, snapshot_.movements, core::Account::Negocio);
}

void MainWindow::persistRepair(const core::Repair& repair) {
    repository_->saveRepair(repair);
    const core::Job* job = snapshot_.job(repair.jobId);
    if (job == nullptr) {
        return;
    }
    const bool closed = repair.status == core::RepairStatus::Cobrada;
    if (job->closed != closed) {
        core::Job updated = *job;
        updated.closed = closed;
        restamp(updated.hlc, updated.deviceId);
        repository_->save(updated);
    }
}

void MainWindow::persistGenerated(core::Movement movement) {
    if (movement.id.empty()) {
        movement.id = stamp(movement.hlc, movement.deviceId);
    } else {
        restamp(movement.hlc, movement.deviceId);
    }
    if (!movement.category.empty() &&
        core::findCategory(snapshot_.categories, movement.category) == nullptr) {
        repository_->saveCategory({movement.category, core::Account::Negocio,
                                   core::CategoryClass::General, movement.kind});
    }
    repository_->save(movement);
}

void MainWindow::newRepair() {
    NewRepairDialog dialog(snapshot_, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto tpl = std::find_if(snapshot_.templates.begin(), snapshot_.templates.end(),
                                  [&dialog](const core::RepairTemplate& t) {
                                      return t.id == dialog.templateId();
                                  });
    if (tpl == snapshot_.templates.end()) {
        return;
    }

    const std::string orderNo = core::nextOrderNumber(snapshot_.repairs);
    const std::string device = dialog.device().toStdString();

    // El trabajo es lo que sincroniza: el telefono lo ve como "R-0042 · RTX
    // 3080". La ficha, con todo lo demas, queda en esta computadora.
    core::Job job;
    job.id = stamp(job.hlc, job.deviceId);
    job.name = orderNo + " · " + device;
    job.client = dialog.client().toStdString();
    job.opened = snapshot_.today;

    const bool ownTx = db_->handle().transaction();
    try {
        repository_->save(job);
        repository_->saveRepair(core::repairFromTemplate(*tpl, job.id, orderNo, device, snapshot_.today));
        for (core::RepairPart part : core::partsFromTemplate(*tpl, job.id)) {
            part.id = storage::newId();
            repository_->saveRepairPart(part);
        }
        repository_->addTiming(QStringLiteral("reparacion"), dialog.elapsedMs());
        if (ownTx && !db_->handle().commit()) {
            throw storage::StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
        }
    } catch (const std::exception& error) {
        if (ownTx) db_->handle().rollback();
        QMessageBox::critical(this, QStringLiteral("No se pudo crear"), QString::fromUtf8(error.what()));
        return;
    }
    afterLocalChange();
    showPage(1);
    repairs_->selectRepair(job.id);
    repairs_->focusList();
    if (dialog.alreadyCharged()) {
        // Un trabajo ya hecho y pagado: horas, precio y fecha, y queda cobrada.
        deliverRepair(job.id);
    }
}

void MainWindow::editRepair(const core::Repair& repair, const QString& client) {
    const core::Job* job = snapshot_.job(repair.jobId);
    if (job == nullptr) {
        return;
    }
    repository_->saveRepair(repair);
    // El nombre y el cliente del trabajo siguen a la ficha: son lo que ve el
    // telefono.
    const std::string name =
        repair.orderNo.empty() ? repair.device : repair.orderNo + " · " + repair.device;
    if (job->client != client.toStdString() || job->name != name) {
        core::Job updated = *job;
        updated.client = client.toStdString();
        updated.name = name;
        restamp(updated.hlc, updated.deviceId);
        repository_->save(updated);
    }
    afterLocalChange();
}

void MainWindow::deliverRepair(const core::Id& jobId) {
    const core::Repair* repair = snapshot_.repair(jobId);
    if (repair == nullptr) {
        return;
    }
    const core::Money price = core::repairPrice(*repair, snapshot_.movements, snapshot_.currency);
    DeliverDialog dialog(*repair, price, snapshot_.today, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto result = core::deliverRepair(*repair, snapshot_.movements, dialog.date(),
                                            dialog.realMinutes(), dialog.priceMinor(),
                                            dialog.charged(), businessPocket(),
                                            kRepairIncomeCategory);
    const bool ownTx = db_->handle().transaction();
    try {
        persistRepair(result.repair);
        if (result.income) {
            persistGenerated(*result.income);
        }
        if (ownTx && !db_->handle().commit()) {
            throw storage::StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
        }
    } catch (const std::exception& error) {
        if (ownTx) db_->handle().rollback();
        QMessageBox::critical(this, QStringLiteral("No se pudo entregar"), QString::fromUtf8(error.what()));
        return;
    }
    afterLocalChange();
}

void MainWindow::chargeRepair(const core::Id& jobId) {
    const core::Repair* repair = snapshot_.repair(jobId);
    if (repair == nullptr) {
        return;
    }
    // Sin entregar todavia: cobrar es entregar y cobrar, y eso pide las horas.
    if (repair->status == core::RepairStatus::EnProceso) {
        deliverRepair(jobId);
        return;
    }
    const auto result = core::chargeRepair(*repair, snapshot_.movements, snapshot_.today,
                                           businessPocket(), kRepairIncomeCategory);
    const bool ownTx = db_->handle().transaction();
    try {
        persistRepair(result.repair);
        if (result.income) {
            persistGenerated(*result.income);
        }
        if (ownTx && !db_->handle().commit()) {
            throw storage::StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
        }
    } catch (const std::exception& error) {
        if (ownTx) db_->handle().rollback();
        QMessageBox::critical(this, QStringLiteral("No se pudo cobrar"), QString::fromUtf8(error.what()));
        return;
    }
    afterLocalChange();
}

void MainWindow::addPart(const core::Id& jobId, const QString& name, qint64 costMinor,
                         bool costKnown, bool bought) {
    const core::Repair* repair = snapshot_.repair(jobId);
    if (repair == nullptr) {
        return;
    }
    core::RepairPart part;
    part.id = storage::newId();
    part.jobId = jobId;
    part.name = name.toStdString();
    part.costMinor = costMinor;
    part.costKnown = costKnown;

    const bool ownTx = db_->handle().transaction();
    try {
        if (bought && costMinor > 0) {
            // Comprado para esta reparacion: el gasto se anota solo y queda
            // enlazado. El costo de la reparacion sale del repuesto, no del
            // gasto, asi que no se cuenta dos veces.
            core::Movement expense;
            expense.date = snapshot_.today;
            expense.name = name.toStdString() + " · " + repair->orderNo;
            expense.kind = core::MovementKind::Gasto;
            expense.amountMinor = costMinor;
            expense.pocketId = businessPocket();
            expense.category = kRepairPartsCategory;
            expense.jobId = jobId;
            expense.id = stamp(expense.hlc, expense.deviceId);
            part.movementId = expense.id;
            persistGenerated(expense);
        }
        repository_->saveRepairPart(part);
        if (ownTx && !db_->handle().commit()) {
            throw storage::StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
        }
    } catch (const std::exception& error) {
        if (ownTx) db_->handle().rollback();
        QMessageBox::critical(this, QStringLiteral("No se pudo agregar"), QString::fromUtf8(error.what()));
        return;
    }
    afterLocalChange();
}

void MainWindow::changePart(const core::RepairPart& part) {
    repository_->saveRepairPart(part);
    // Si el repuesto tiene su gasto, el gasto sigue al costo corregido.
    if (!part.movementId.empty()) {
        const auto it = std::find_if(snapshot_.movements.begin(), snapshot_.movements.end(),
                                     [&part](const core::Movement& m) { return m.id == part.movementId; });
        if (it != snapshot_.movements.end() && it->amountMinor != part.costMinor && part.costMinor > 0) {
            core::Movement updated = *it;
            updated.amountMinor = part.costMinor;
            persistGenerated(updated);
        }
    }
    afterLocalChange();
}

void MainWindow::removePart(const core::RepairPart& part) {
    const bool ownTx = db_->handle().transaction();
    try {
        repository_->removeRepairPart(part.id);
        if (!part.movementId.empty()) {
            const auto it = std::find_if(snapshot_.movements.begin(), snapshot_.movements.end(),
                                         [&part](const core::Movement& m) { return m.id == part.movementId; });
            if (it != snapshot_.movements.end()) {
                repository_->remove(*it);
            }
        }
        if (ownTx && !db_->handle().commit()) {
            throw storage::StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
        }
    } catch (const std::exception& error) {
        if (ownTx) db_->handle().rollback();
        QMessageBox::critical(this, QStringLiteral("No se pudo quitar"), QString::fromUtf8(error.what()));
        return;
    }
    afterLocalChange();
}

void MainWindow::reconcile(const core::Id& pocketId) {
    const core::Pocket* pocket = snapshot_.pocket(pocketId);
    if (pocket == nullptr) {
        return;
    }

    const auto balances = core::pocketBalances(snapshot_.pockets, snapshot_.movements,
                                               snapshot_.currency, snapshot_.today);
    const auto it = std::find_if(
        balances.begin(), balances.end(),
        [&pocketId](const core::PocketBalance& balance) { return balance.pocketId == pocketId; });
    if (it == balances.end()) {
        return;
    }

    ReconcileDialog dialog(*pocket, it->balance, snapshot_.today, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    // Cuadrado hoy, haya diferencia o no: es lo que saca el pendiente.
    repository_->setSetting(QStringLiteral("bolsillo.%1.cuadrado").arg(QString::fromStdString(pocketId)),
                            QString::fromStdString(snapshot_.today.toIso()));
    if (const auto adjustment = dialog.result()) {
        core::Movement saved = *adjustment;
        saved.id = stamp(saved.hlc, saved.deviceId);
        repository_->save(saved);
    }
    afterLocalChange();
}

void MainWindow::editMovement(const core::Id& movementId) {
    const auto it =
        std::find_if(snapshot_.movements.begin(), snapshot_.movements.end(),
                     [&movementId](const core::Movement& m) { return m.id == movementId; });
    if (it == snapshot_.movements.end()) {
        return;
    }

    MovementEditor editor(*it, snapshot_, this);
    if (editor.exec() != QDialog::Accepted) {
        return;
    }

    if (editor.wasDeleted()) {
        const core::Movement borrado = *it;
        repository_->remove(borrado);
        rememberUndo(borrado, borrado,
                     QStringLiteral("borrar «%1»").arg(QString::fromStdString(borrado.name)));
        afterLocalChange();
        return;
    }

    const core::Movement previo = *it;
    core::Movement updated = editor.result();
    restamp(updated.hlc, updated.deviceId);
    try {
        repository_->save(updated);
    } catch (const std::exception& error) {
        QMessageBox::critical(this, QStringLiteral("No se pudo guardar"),
                              QString::fromUtf8(error.what()));
        return;
    }
    rememberUndo(previo, updated,
                 QStringLiteral("editar «%1»").arg(QString::fromStdString(updated.name)));
    afterLocalChange();
}

void MainWindow::deleteEverything() {
    // El aviso nombra el archivo y el telefono a proposito. La version
    // anterior de este boton decia que la base real no se tocaba —cierto
    // cuando esto era un banco de pruebas, falso desde que es la aplicacion— y
    // un aviso que tranquiliza sobre algo que ya no es verdad es peor que no
    // tener aviso.
    const auto answer = QMessageBox::warning(
        this, QStringLiteral("Borrar todo"),
        QStringLiteral("Esto borra TODOS tus bolsillos, trabajos y movimientos.\n\n"
                       "%1\n\n"
                       "El borrado se sincroniza: también desaparecen del teléfono y "
                       "del servidor la próxima vez que se conecten. No hay forma de "
                       "deshacerlo desde la aplicación.")
            .arg(db_->path()),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }

    try {
        const std::size_t borrados = repository_->deleteEverything();
        afterLocalChange();
        QMessageBox::information(this, QStringLiteral("Borrado"),
                                 QStringLiteral("Se borraron %1 registros.")
                                     .arg(borrados));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, QStringLiteral("No se pudo borrar"),
                              QString::fromUtf8(error.what()));
    }
}

// -------------------------------------------------------------------- Nube

void MainWindow::toggleSignIn() {
    if (supabase_->isSignedIn()) {
        supabase_->signOut();
        repository_->setSetting(QStringLiteral("sync.refresh_token"), QString());
        updateCloudUi(QStringLiteral("Sin sesión"));
        return;
    }

    bool ok;
    QString email = QInputDialog::getText(this, QStringLiteral("Conectar"),
                                          QStringLiteral("Correo:"), QLineEdit::Normal,
                                          QString(), &ok);
    if (!ok || email.isEmpty()) {
        return;
    }

    QString pwd = QInputDialog::getText(this, QStringLiteral("Conectar"),
                                        QStringLiteral("Contraseña:"), QLineEdit::Password,
                                        QString(), &ok);
    if (!ok || pwd.isEmpty()) {
        return;
    }

    // trimmed() en el correo: un espacio al final, que es trivial al escribir o
    // al pegar, da exactamente el mismo "invalid login credentials" que una
    // contrasena equivocada, y no hay forma de que el usuario lo note. La
    // contrasena NO se toca: un espacio ahi puede ser parte de la contrasena.
    supabase_->signInWithPassword(email.trimmed(), pwd);
}

void MainWindow::syncNow() {
    if (!supabase_->isSignedIn()) {
        toggleSignIn();
        return;
    }
    lastSyncWasAutomatic_ = false;
    syncEngine_->sync();
    updateCloudUi(cloudStatus_->text());
}

void MainWindow::undoLast() {
    if (!undoAfter_) {
        QMessageBox::information(this, QStringLiteral("Deshacer"),
                                 QStringLiteral("No hay nada que deshacer en esta sesión."));
        return;
    }

    try {
        if (undoBefore_) {
            // Existia antes: vuelve como estaba. Con hlc nuevo, porque el
            // cambio que estamos deshaciendo ya subio o esta por subir, y el
            // que gana en el otro aparato es el de reloj mas alto.
            core::Movement previo = *undoBefore_;
            restamp(previo.hlc, previo.deviceId);
            repository_->save(previo);
        } else {
            // No existia: deshacer es borrarlo. remove() deja lapida, asi que
            // el borrado tambien viaja.
            repository_->remove(*undoAfter_);
            for (const core::Movement& also : undoAlso_) {
                repository_->remove(also);
            }
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(this, QStringLiteral("No se pudo deshacer"),
                              QString::fromUtf8(error.what()));
        return;
    }

    const QString hecho = undoLabel_;
    undoBefore_.reset();
    undoAfter_.reset();
    undoAlso_.clear();
    undoLabel_.clear();

    afterLocalChange();
    updateCloudUi(QStringLiteral("Se deshizo: ") + hecho);
}

void MainWindow::afterLocalChange() {
    reload();
    if (supabase_->isSignedIn()) {
        // start() sobre un temporizador andando lo reinicia desde cero: esa es
        // toda la agrupacion. No hace falta contar cambios ni acumular nada.
        autoSyncTimer_->start();
    }
}

void MainWindow::runAutoSync() {
    if (!supabase_->isSignedIn()) {
        return;
    }
    if (syncEngine_->isRunning()) {
        // Volver a esperar en vez de encolar una segunda. Lo que se guardo
        // recien ya esta en la cola de salida, asi que no se pierde: lo sube
        // esta pasada si llega a tiempo, o la proxima.
        autoSyncTimer_->start();
        return;
    }
    lastSyncWasAutomatic_ = true;
    syncEngine_->sync();
}

void MainWindow::updateCloudUi(const QString& message) {
    if (!cloudButton_ || !syncButton_ || !cloudStatus_) {
        return;
    }

    if (supabase_->isSignedIn()) {
        cloudButton_->setText(QStringLiteral("Desconectar"));
        syncButton_->setEnabled(!syncEngine_->isRunning());
    } else {
        cloudButton_->setText(QStringLiteral("Conectar"));
        syncButton_->setEnabled(false);
    }
    cloudStatus_->setText(message);
}

} // namespace dake::ui
