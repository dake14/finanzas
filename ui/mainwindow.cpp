#include "mainwindow.hpp"

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QButtonGroup>
#include <QDate>
#include <QDateTime>
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
#include "dake/sync/config.hpp"
#include "dialogs.hpp"
#include "pages.hpp"
#include "capturewidget.hpp"
#include "capturewindow.hpp"
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
    syncEngine_ = std::make_unique<sync::SyncEngine>(*supabase_, *repository_, this);

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
    for (const QString& candidate : candidates) {
        if (applyHotkey(QKeySequence(candidate, QKeySequence::PortableText))) {
            break;
        }
    }
    // Por defecto arranca con Windows: el atajo no sirve si la aplicacion no
    // esta abierta. Se reescribe en cada arranque para que siga la ruta del
    // ejecutable si se movio de carpeta.
    applyAutostart(repository_->setting(QStringLiteral("config.arranque")).value_or(QStringLiteral("1")) ==
                   QStringLiteral("1"));

    reload();

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

/// El icono de la bandeja: una "F" sobre el cian del logo. Pintado y no
/// cargado de un archivo, para no depender de recursos.
[[nodiscard]] QIcon trayIcon() {
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(theme::kAccent);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRectF(2, 2, 60, 60), 14, 14);
    painter.setPen(theme::kBackground);
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
    setStyleSheet(theme::styleSheet());

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
    jobs_ = new JobsPage(stack_);
    movements_ = new MovementsPage(stack_);
    pockets_ = new PocketsPage(stack_);
    closing_ = new ClosingPage(stack_);
    reports_ = new ReportsPage(stack_);
    settings_ = new SettingsPage(stack_);
    // El orden importa: es el mismo que el de los botones de la barra y el que
    // usa showPage().
    for (QWidget* page : {static_cast<QWidget*>(today_), static_cast<QWidget*>(jobs_),
                          static_cast<QWidget*>(movements_), static_cast<QWidget*>(pockets_),
                          static_cast<QWidget*>(closing_), static_cast<QWidget*>(reports_),
                          static_cast<QWidget*>(settings_)}) {
        stack_->addWidget(page);
    }
    root->addWidget(stack_, 1);
    setCentralWidget(central);

    // Las dos capturas —la de Hoy y la ventana mini— guardan igual. Lo unico
    // distinto es que la mini se esconde al guardar, salvo con Shift+Enter.
    const auto onCapture = [this](const core::Movement& movement,
                                  const core::Category& newCategory, qint64 elapsedMs) {
        addMovement(movement, newCategory);
        if (elapsedMs > 0) {
            repository_->addTiming(QStringLiteral("captura"), elapsedMs);
        }
    };
    connect(today_->capture(), &CaptureWidget::submitted, this,
            [onCapture](const core::Movement& m, const core::Category& c, qint64 ms, bool) {
                onCapture(m, c, ms);
            });

    captureWindow_ = new CaptureWindow();
    connect(captureWindow_->capture(), &CaptureWidget::submitted, this,
            [this, onCapture](const core::Movement& m, const core::Category& c, qint64 ms,
                              bool keepOpen) {
                if (!keepOpen) {
                    captureWindow_->hide();
                }
                onCapture(m, c, ms);
                if (tray_ != nullptr && !keepOpen && !isVisible()) {
                    tray_->showMessage(QStringLiteral("Anotado"),
                                       QString::fromStdString(m.name) + QStringLiteral(" · ") +
                                           theme::formatMoney(core::Money::fromMinor(
                                               m.amountMinor, snapshot_.currency)),
                                       QSystemTrayIcon::Information, 2500);
                }
            });
    connect(captureWindow_->capture(), &CaptureWidget::cancelled, captureWindow_,
            &QWidget::hide);

    hotkey_ = new GlobalHotkey(this);
    connect(hotkey_, &GlobalHotkey::activated, this, &MainWindow::showCapture);
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
    connect(jobs_, &JobsPage::newJobRequested, this, &MainWindow::newJob);
    connect(jobs_, &JobsPage::jobActivated, this, &MainWindow::toggleJob);
    connect(movements_, &MovementsPage::movementActivated, this, &MainWindow::editMovement);
    connect(pockets_, &PocketsPage::newPocketRequested, this, &MainWindow::newPocket);
    connect(pockets_, &PocketsPage::reconcileRequested, this, &MainWindow::reconcile);
    // Encolados: los dos vienen de un boton o un desplegable que la recarga
    // posterior destruye, y destruirlo dentro de su propia senal cuelga la app.
    connect(pockets_, &PocketsPage::accountToggled, this, &MainWindow::togglePocketAccount,
            Qt::QueuedConnection);
    connect(settings_, &SettingsPage::categoryChanged, this, &MainWindow::saveCategory,
            Qt::QueuedConnection);

    // Ctrl+1 a Ctrl+7: cada seccion a una tecla, para no tocar el mouse.
    for (int index = 0; index < 7; ++index) {
        auto* go = new QShortcut(QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + index)),
                                 this);
        connect(go, &QShortcut::activated, this, [this, index] { showPage(index); });
    }

    // Ctrl+N va a anotar y Ctrl+F a buscar. Son los dos verbos de la
    // aplicacion; el resto se puede alcanzar con el mouse sin que duela.
    auto* newShortcut = new QShortcut(QKeySequence::New, this);
    connect(newShortcut, &QShortcut::activated, this, [this] {
        showPage(0);
        today_->capture()->focusInput();
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

    auto* brand = new QLabel(QStringLiteral("Finanzas"), parent);
    brand->setFont(theme::displayFont(14, QFont::Bold));
    theme::setLabelColor(brand, theme::kText);
    layout->addWidget(brand);

    auto* subtitle = new QLabel(QStringLiteral("DakeLabs"), parent);
    subtitle->setFont(theme::bodyFont(9));
    theme::setLabelColor(subtitle, theme::kAccent);
    layout->addWidget(subtitle);
    layout->addSpacing(18);

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    const QStringList names{QStringLiteral("Hoy"), QStringLiteral("Trabajos"),
                            QStringLiteral("Movimientos"), QStringLiteral("Bolsillos"),
                            QStringLiteral("Cierre"), QStringLiteral("Reportes"),
                            QStringLiteral("Ajustes")};
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
    for (int i = 0; i < navButtons_.size(); ++i) {
        navButtons_[i]->setChecked(i == index);
    }
}

void MainWindow::reload() {
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

    today_->setSnapshot(snapshot_);
    jobs_->setSnapshot(snapshot_);
    movements_->setSnapshot(snapshot_);
    pockets_->setSnapshot(snapshot_);
    closing_->setSnapshot(snapshot_);
    snapshot_.hotkey = hotkey_ != nullptr ? hotkey_->shortcut().toString(QKeySequence::PortableText)
                                          : QString();
    snapshot_.hotkeyRegistered = hotkey_ != nullptr && hotkey_->isRegistered();
    snapshot_.autostart = repository_->setting(QStringLiteral("config.arranque"))
                              .value_or(QStringLiteral("1")) == QStringLiteral("1");
    snapshot_.captureMedianMs = repository_->timingMedian(QStringLiteral("captura"));

    reports_->setSnapshot(snapshot_);
    settings_->setSnapshot(snapshot_);
    if (captureWindow_ != nullptr) {
        captureWindow_->capture()->setSnapshot(snapshot_);
    }

    footer_->setText(db_->path());
}

// ---------------------------------------------------------------- Acciones

void MainWindow::addMovement(const core::Movement& draft, const core::Category& newCategory) {
    const core::Account account = !newCategory.name.empty()
                                      ? newCategory.account
                                      : snapshot_.categoryAccount(draft.category, draft.pocketId);

    // Un gasto personal pagado con plata del negocio se guarda como lo que es:
    // un sueldo y un gasto personal. El negocio no registra un almuerzo.
    std::vector<core::Movement> parts =
        core::splitCrossExpense(draft, snapshot_.pockets, account, snapshot_.personalPocket());
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
        if (ownTx && !db_->handle().commit()) {
            throw storage::StorageError(QStringLiteral("No se pudo confirmar la transaccion."));
        }
    } catch (const std::exception& error) {
        if (ownTx) db_->handle().rollback();
        QMessageBox::critical(this, QStringLiteral("No se pudo guardar"),
                              QString::fromUtf8(error.what()));
        return;
    }

    const core::Movement& main = parts.back();
    rememberUndo(std::nullopt, main,
                 QStringLiteral("anotar «%1»").arg(QString::fromStdString(main.name)));
    undoAlso_.assign(parts.begin(), parts.end() - 1);
    afterLocalChange();
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

void MainWindow::newJob() {
    JobDialog dialog(snapshot_.today, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto job = dialog.result();
    if (!job) {
        return;
    }
    core::Job saved = *job;
    saved.id = stamp(saved.hlc, saved.deviceId);
    repository_->save(saved);
    afterLocalChange();
}

void MainWindow::toggleJob(const core::Id& jobId) {
    const auto it = std::find_if(snapshot_.jobs.begin(), snapshot_.jobs.end(),
                                 [&jobId](const core::Job& job) { return job.id == jobId; });
    if (it == snapshot_.jobs.end()) {
        return;
    }

    const QString question =
        it->closed ? QStringLiteral("¿Reabrir \"%1\"?") : QStringLiteral("¿Dar por cerrado \"%1\"?");
    const auto answer =
        QMessageBox::question(this, QStringLiteral("Trabajo"),
                              question.arg(QString::fromStdString(it->name)) +
                                  QStringLiteral("\n\nUn trabajo cerrado deja de ofrecerse al "
                                                 "cargar movimientos, pero sigue contando en "
                                                 "el historial."),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) {
        return;
    }

    core::Job updated = *it;
    updated.closed = !updated.closed;
    updated.hlc = clock(deviceId_.toStdString()).now(QDateTime::currentMSecsSinceEpoch()).encode();
    updated.deviceId = deviceId_.toStdString();
    repository_->save(updated);
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
    const auto adjustment = dialog.result();
    if (!adjustment) {
        return;
    }
    core::Movement saved = *adjustment;
    saved.id = stamp(saved.hlc, saved.deviceId);
    repository_->save(saved);
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
