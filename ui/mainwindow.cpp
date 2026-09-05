#include "mainwindow.hpp"

#include <QAction>
#include <QButtonGroup>
#include <QDate>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QLineEdit>

#include <algorithm>

#include "dake/core/hlc.hpp"
#include "dake/sync/config.hpp"
#include "dialogs.hpp"
#include "pages.hpp"
#include "quickentry.hpp"
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

    // Un banco de pruebas que arranca en blanco no se puede evaluar: no hay
    // contra que comparar. Se siembra el caso real de agosto, y el boton de
    // "volver a foja cero" esta a la vista para empezar de nuevo cuando se
    // quiera.
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

    buildUi();
    reload();

    const auto token = repository_->setting(QStringLiteral("sync.refresh_token"));
    if (token) {
        supabase_->restoreSession(*token);
    }
}

MainWindow::~MainWindow() = default;

core::Id MainWindow::stamp(std::string& hlc, std::string& deviceId) {
    hlc = clock(deviceId_.toStdString()).now(QDateTime::currentMSecsSinceEpoch()).encode();
    deviceId = deviceId_.toStdString();
    return storage::newId();
}

// --------------------------------------------------------------------- UI

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("Banco de pruebas · Finanzas DakeLabs"));
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
    for (QWidget* page : {static_cast<QWidget*>(today_), static_cast<QWidget*>(jobs_),
                          static_cast<QWidget*>(movements_), static_cast<QWidget*>(pockets_)}) {
        stack_->addWidget(page);
    }
    root->addWidget(stack_, 1);
    setCentralWidget(central);

    connect(today_->quickEntry(), &QuickEntry::submitted, this, &MainWindow::addMovement);
    connect(jobs_, &JobsPage::newJobRequested, this, &MainWindow::newJob);
    connect(jobs_, &JobsPage::jobActivated, this, &MainWindow::toggleJob);
    connect(movements_, &MovementsPage::movementActivated, this, &MainWindow::editMovement);
    connect(pockets_, &PocketsPage::newPocketRequested, this, &MainWindow::newPocket);
    connect(pockets_, &PocketsPage::reconcileRequested, this, &MainWindow::reconcile);

    // Ctrl+N va a anotar y Ctrl+F a buscar. Son los dos verbos de la
    // aplicacion; el resto se puede alcanzar con el mouse sin que duela.
    auto* newShortcut = new QShortcut(QKeySequence::New, this);
    connect(newShortcut, &QShortcut::activated, this, [this] {
        showPage(0);
        today_->quickEntry()->focusName();
    });
    auto* findShortcut = new QShortcut(QKeySequence::Find, this);
    connect(findShortcut, &QShortcut::activated, this, [this] {
        showPage(2);
        movements_->focusSearch();
    });

    auto* syncShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this);
    connect(syncShortcut, &QShortcut::activated, this, &MainWindow::syncNow);

    showPage(0);
}

void MainWindow::buildSidebar(QWidget* parent) {
    auto* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(16, 22, 16, 18);
    layout->setSpacing(6);

    auto* brand = new QLabel(QStringLiteral("Banco de pruebas"), parent);
    brand->setFont(theme::displayFont(14, QFont::Bold));
    theme::setLabelColor(brand, theme::kText);
    layout->addWidget(brand);

    auto* subtitle = new QLabel(QStringLiteral("Finanzas DakeLabs"), parent);
    subtitle->setFont(theme::bodyFont(9));
    theme::setLabelColor(subtitle, theme::kAccent);
    layout->addWidget(subtitle);
    layout->addSpacing(18);

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    const QStringList names{QStringLiteral("Hoy"), QStringLiteral("Trabajos"),
                            QStringLiteral("Movimientos"), QStringLiteral("Bolsillos")};
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

    auto* reset = new QPushButton(QStringLiteral("Volver al caso de agosto"), parent);
    reset->setCursor(Qt::PointingHandCursor);
    reset->setFont(theme::bodyFont(9));
    reset->setToolTip(QStringLiteral("Borra todo y vuelve a cargar el caso de prueba."));
    connect(reset, &QPushButton::clicked, this, &MainWindow::resetToSeed);
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

    today_->setSnapshot(snapshot_);
    jobs_->setSnapshot(snapshot_);
    movements_->setSnapshot(snapshot_);
    pockets_->setSnapshot(snapshot_);

    footer_->setText(QStringLiteral("Base de pruebas, separada de la real:\n") + db_->path());
}

// ---------------------------------------------------------------- Acciones

void MainWindow::addMovement(const core::Movement& draft) {
    core::Movement movement = draft;
    movement.id = stamp(movement.hlc, movement.deviceId);
    try {
        repository_->save(movement);
    } catch (const std::exception& error) {
        QMessageBox::critical(this, QStringLiteral("No se pudo guardar"),
                              QString::fromUtf8(error.what()));
        return;
    }
    afterLocalChange();
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
        repository_->remove(*it);
        afterLocalChange();
        return;
    }

    core::Movement updated = editor.result();
    updated.hlc = clock(deviceId_.toStdString()).now(QDateTime::currentMSecsSinceEpoch()).encode();
    updated.deviceId = deviceId_.toStdString();
    try {
        repository_->save(updated);
    } catch (const std::exception& error) {
        QMessageBox::critical(this, QStringLiteral("No se pudo guardar"),
                              QString::fromUtf8(error.what()));
        return;
    }
    afterLocalChange();
}

void MainWindow::resetToSeed() {
    const auto answer = QMessageBox::warning(
        this, QStringLiteral("Volver al caso de agosto"),
        QStringLiteral("Esto borra todo lo cargado en el banco de pruebas y vuelve a poner el "
                       "caso de agosto de 2026.\n\nLa base de la aplicacion real no se toca: "
                       "es otro archivo."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }
    repository_->wipe();
    repository_->seedIfEmpty(snapshot_.currency);
    afterLocalChange();
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
