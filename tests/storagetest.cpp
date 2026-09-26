// tests/storagetest.cpp — la base guarda y devuelve exactamente lo mismo, y el
// archivo de intercambio lleva los datos de un equipo al otro sin perder nada.
//
// Trabaja siempre sobre un archivo temporal propio. Nunca toca la base real de
// la aplicacion ni la del banco de pruebas: una suite que escribe sobre datos
// de verdad se deja de correr a la semana, que es la peor forma de no tener
// pruebas.

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QVariant>
#include <QTemporaryDir>

#include <algorithm>
#include <cstdio>
#include <string>

#include "dake/core/demo.hpp"
#include "dake/core/report.hpp"
#include "dake/storage/exchange.hpp"
#include "dake/storage/repository.hpp"

namespace {

int gFailures = 0;

void check(bool condition, const std::string& what) {
    std::printf("%s %s\n", condition ? "ok   " : "FALLA", what.c_str());
    if (!condition) {
        ++gFailures;
    }
}

void checkMinor(std::int64_t got, std::int64_t want, const std::string& what) {
    if (got == want) {
        std::printf("ok    %s (%lld)\n", what.c_str(), static_cast<long long>(got));
    } else {
        std::printf("FALLA %s: esperaba %lld y salio %lld\n", what.c_str(),
                    static_cast<long long>(want), static_cast<long long>(got));
        ++gFailures;
    }
}

using namespace dake;

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    QTemporaryDir temp;
    if (!temp.isValid()) {
        std::printf("FALLA no se pudo crear la carpeta temporal\n");
        return 1;
    }
    const QString path = temp.path() + QStringLiteral("/pruebas.db");
    const auto currency = core::Currency::usd();

    std::printf("Banco de pruebas — verificacion de la base\n(%s)\n\n",
                path.toStdString().c_str());

    // --- Siembra e ida y vuelta -------------------------------------------
    {
        storage::Database db(path);
        storage::Repository repository(db);

        check(repository.isEmpty(), "una base recien creada esta vacia");
        check(repository.seedIfEmpty(currency), "la siembra corre una vez");
        check(!repository.seedIfEmpty(currency), "y no vuelve a correr sobre datos existentes");

        const auto pockets = repository.loadPockets();
        const auto jobs = repository.loadJobs();
        const auto movements = repository.loadMovements();
        const auto original = core::realCaseAugust2026(currency);

        check(pockets.size() == original.pockets.size(), "vuelven todos los bolsillos");
        check(jobs.size() == original.jobs.size(), "vuelven todos los trabajos");
        check(movements.size() == original.movements.size(), "vuelven todos los movimientos");

        // Lo que importa no es que vuelvan las filas, sino que vuelvan IGUALES.
        // Un traspaso al que la base le pierde el bolsillo de destino sigue
        // siendo una fila, y deja de ser un traspaso.
        const auto memoria =
            core::funding(original.pockets, original.movements, currency,
                          core::Date::fromIso("2026-08-01"), core::Date::fromIso("2026-08-31"));
        const auto disco =
            core::funding(pockets, movements, currency, core::Date::fromIso("2026-08-01"),
                          core::Date::fromIso("2026-08-31"));
        checkMinor(disco.net.minor(), memoria.net.minor(),
                   "el financiamiento calculado desde disco es identico al de memoria");

        const auto balances = core::pocketBalances(pockets, movements, currency,
                                                   core::Date::fromIso("2026-08-31"));
        checkMinor(core::totalFor(balances, core::PocketKind::Operacion, currency).minor(),
                   81'58, "la caja del negocio vuelve con su saldo");
        checkMinor(core::totalFor(balances, core::PocketKind::Ahorro, currency).minor(), 350'00,
                   "y el ahorro con el suyo");

        bool spreadSurvives = false;
        bool unsettledSurvives = false;
        for (const core::Movement& movement : movements) {
            if (movement.id == "m-08") {
                spreadSurvives = movement.spreadMonths == 4;
            }
            if (movement.id == "m-09") {
                unsettledSurvives = !movement.settled;
            }
        }
        check(spreadSurvives, "los meses que dura una compra sobreviven al viaje a la base");
        check(unsettledSurvives, "y la marca de 'sin cobrar' tambien");
    }

    // --- Una forma invalida no llega a escribirse -------------------------
    {
        storage::Database db(path);
        storage::Repository repository(db);

        core::Movement roto;
        roto.id = "roto";
        roto.date = core::Date::fromIso("2026-08-30");
        roto.name = "Traspaso sin destino";
        roto.kind = core::MovementKind::Traspaso;
        roto.amountMinor = 1000;
        roto.pocketId = "p-caja";
        // targetPocketId queda vacio a proposito.

        bool rejected = false;
        try {
            repository.save(roto);
        } catch (const std::exception&) {
            rejected = true;
        }
        check(rejected, "un traspaso sin destino se rechaza ANTES de tocar la base");
        check(repository.loadMovements().size() == 10, "y no quedo ninguna fila de mas");
    }

    // --- Lapidas y persistencia entre sesiones ----------------------------
    std::string removedId;
    {
        {
            storage::Database db(path);
            storage::Repository repository(db);
            const auto movements = repository.loadMovements();
            removedId = movements.back().id;
            repository.remove(movements.back());
            check(repository.loadMovements().size() == 9, "el borrado saca la fila de la lista");
        }
        {
            // Se reabre la base: lo borrado sigue borrado y lo demas sigue ahi.
            storage::Database db(path);
            storage::Repository repository(db);
            const auto movements = repository.loadMovements();
            check(movements.size() == 9, "al reabrir, la lapida sigue puesta");
            bool found = false;
            for (const core::Movement& movement : movements) {
                if (movement.id == removedId) {
                    found = true;
                }
            }
            check(!found, "y el movimiento borrado no vuelve");
        }
    }

    // --- El puente entre el telefono y la computadora ---------------------
    {
        const QString archivo = temp.path() + QStringLiteral("/cambios.jsonl");
        const QString otraBase = temp.path() + QStringLiteral("/otra.db");

        storage::Database db(path);
        storage::Repository origen(db);

        const int escritos = storage::exportAll(origen, archivo);
        checkMinor(escritos, 4 + 3 + 10, "se exportan bolsillos, trabajos y movimientos");
        check(QFile::exists(archivo), "el archivo queda escrito");

        // Una base virgen: es el equipo del otro lado.
        storage::Database db2(otraBase);
        storage::Repository destino(db2);
        check(destino.isEmpty(), "la base de destino arranca vacia");

        const auto primera = storage::importFile(destino, archivo);
        checkMinor(primera.applied, 17, "la primera importacion aplica todo");
        checkMinor(primera.malformed, 0, "sin lineas rotas");
        check(destino.loadMovements().size() == 9,
              "llegan los movimientos vivos, y el borrado no aparece en la lista");

        bool lapidaViajo = false;
        for (const core::Movement& movement : destino.loadMovements(true)) {
            if (movement.id == removedId && movement.deleted) {
                lapidaViajo = true;
            }
        }
        check(lapidaViajo, "la lapida viaja: lo borrado en un equipo no revive en el otro");

        // Idempotencia: aplicar el mismo archivo otra vez no cambia nada. Es lo
        // que permite reimportar sin acordarse de si ya se hizo.
        const auto segunda = storage::importFile(destino, archivo);
        checkMinor(segunda.applied, 0, "la segunda importacion no aplica nada");
        checkMinor(segunda.skippedOlder, 17, "y descarta todo por no ser mas nuevo");
        check(destino.loadMovements().size() == 9, "el estado queda identico");

        // Gana el HLC mas alto, no el que llega ultimo.
        auto editado = destino.loadMovements().front();
        const std::string editadoId = editado.id;
        editado.name = "editado en el telefono";
        editado.hlc = "9999999999999-00000-telefono";
        destino.save(editado);

        const auto tercera = storage::importFile(destino, archivo);
        checkMinor(tercera.skippedOlder, 17, "el archivo viejo no pisa la edicion mas nueva");

        bool conservado = false;
        for (const core::Movement& movement : destino.loadMovements()) {
            if (movement.id == editadoId && movement.name == "editado en el telefono") {
                conservado = true;
            }
        }
        check(conservado, "y la edicion mas nueva sobrevive");

        // Una linea rota se cuenta y no se lleva puesto el resto del archivo.
        const QString sucio = temp.path() + QStringLiteral("/sucio.jsonl");
        {
            QFile fuente(archivo);
            fuente.open(QIODevice::ReadOnly);
            const QByteArray bueno = fuente.readAll();

            QFile destinoSucio(sucio);
            destinoSucio.open(QIODevice::WriteOnly);
            destinoSucio.write("{esto no es json");
            destinoSucio.write("\n");
            destinoSucio.write(bueno);
            destinoSucio.write("tampoco esto");
            destinoSucio.write("\n");
        }

        storage::Database db3(temp.path() + QStringLiteral("/tercera.db"));
        storage::Repository tercero(db3);
        const auto conBasura = storage::importFile(tercero, sucio);
        checkMinor(conBasura.malformed, 2, "las dos lineas rotas se cuentan");
        checkMinor(conBasura.applied, 17, "y los 17 registros buenos entran igual");
    }

    // --- Vaciar y volver a sembrar ----------------------------------------
    {
        storage::Database db(path);
        storage::Repository repository(db);
        repository.wipe();
        check(repository.isEmpty(), "el borrado total deja la base vacia");
        check(repository.seedIfEmpty(currency), "y se puede volver a sembrar");
        check(repository.loadMovements().size() == 10, "con el caso completo otra vez");
    }

    // --- Borrar todo SI viaja ---------------------------------------------
    //
    // La diferencia con wipe() es la unica que importa: deleteEverything deja
    // lapida y encola, asi que el borrado llega al otro aparato. Un borrado que
    // no encola se ve igual de bien en esta maquina y no borra nada en la otra,
    // y eso no se nota hasta que se abre el telefono.
    {
        storage::Database db(path + QStringLiteral(".borrado"));
        storage::Repository repository(db);
        repository.seedIfEmpty(currency);
        std::vector<qint64> yaSubidos;
        for (const auto& fila : repository.pendingOutbox(1000)) {
            yaSubidos.push_back(fila.rowId);
        }
        repository.markOutboxSent(yaSubidos);
        checkMinor(repository.pendingOutboxCount(), 0, "la cola arranca limpia");

        const std::size_t vivos = repository.loadPockets().size() +
                                  repository.loadJobs().size() +
                                  repository.loadMovements().size();
        const std::size_t borrados = repository.deleteEverything();

        checkMinor(static_cast<int>(borrados), static_cast<int>(vivos),
                   "devuelve cuantos borro");
        check(repository.loadMovements().empty(), "no queda ningun movimiento a la vista");
        check(repository.loadJobs().empty(), "ni trabajos");
        check(repository.loadPockets().empty(), "ni bolsillos");
        checkMinor(repository.pendingOutboxCount(), static_cast<int>(vivos),
                   "y cada uno dejo su fila para subir");
    }

    // --- Migracion de la version 2 a la 3 ---------------------------------
    //
    // Se arma una base "de la version 2" a mano: la de hoy, sin las tablas
    // locales y con user_version = 2. Es lo que tiene David en su equipo. Al
    // abrirla tiene que quedar con las tablas nuevas, sin perder un solo
    // movimiento, y con el respaldo al lado.
    {
        const QString viejaPath = path + QStringLiteral(".v2");
        {
            storage::Database db(viejaPath);
            storage::Repository repository(db);
            repository.seedIfEmpty(currency);
            QSqlQuery q(db.handle());
            for (const char* tabla : {"pocket_meta", "categories", "movement_meta", "repairs",
                                      "repair_parts", "repair_templates", "recurring", "tools",
                                      "timings"}) {
                q.exec(QStringLiteral("DROP TABLE IF EXISTS %1").arg(QString::fromLatin1(tabla)));
            }
            q.exec(QStringLiteral("PRAGMA user_version = 2"));
        }
        storage::Database db(viejaPath);
        storage::Repository repository(db);
        QSqlQuery version(db.handle());
        version.exec(QStringLiteral("PRAGMA user_version"));
        version.next();
        checkMinor(version.value(0).toInt(), 3, "la base vieja sube a la version 3");
        check(repository.loadMovements().size() == 10, "sin perder ningun movimiento");
        QSqlQuery tabla(db.handle());
        tabla.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name IN "
            "('pocket_meta','categories','movement_meta','repairs','repair_parts',"
            "'repair_templates','recurring','tools','timings')"));
        tabla.next();
        checkMinor(tabla.value(0).toInt(), 9, "con las nueve tablas locales creadas");
        const QStringList respaldos =
            QDir(QFileInfo(viejaPath).absolutePath())
                .entryList({QFileInfo(viejaPath).fileName() + QStringLiteral(".v2-*.bak")});
        check(!respaldos.isEmpty(), "y con el respaldo de la version 2 al lado");
    }

    // --- La cuenta de cada bolsillo ---------------------------------------
    {
        storage::Database db(path + QStringLiteral(".cuentas"));
        storage::Repository repository(db);
        repository.seedIfEmpty(currency);
        const auto antes = repository.loadPockets();
        const auto ahorro = std::find_if(antes.begin(), antes.end(), [](const core::Pocket& p) {
            return p.kind == core::PocketKind::Ahorro;
        });
        check(ahorro != antes.end() && !ahorro->accountOverride,
              "sin marca, un bolsillo no trae cuenta elegida a mano");

        repository.setPocketAccount(ahorro->id, core::Account::Personal);
        auto marcado = repository.loadPockets();
        auto it = std::find_if(marcado.begin(), marcado.end(),
                               [&](const core::Pocket& p) { return p.id == ahorro->id; });
        check(it != marcado.end() && it->accountOverride == core::Account::Personal,
              "la cuenta elegida a mano vuelve de la base");
        check(it != marcado.end() && core::accountOf(*it) == core::Account::Personal,
              "y manda sobre el tipo");

        // Bajar del servidor reemplaza la fila de pockets entera. La marca
        // vive al costado y tiene que sobrevivir.
        core::Pocket remoto = *it;
        remoto.accountOverride.reset();
        remoto.name = "Ahorro renombrado en el telefono";
        remoto.hlc = "9999999999999-00000-telefono";
        check(repository.applyRemote(remoto), "llega un cambio del telefono");
        marcado = repository.loadPockets();
        it = std::find_if(marcado.begin(), marcado.end(),
                          [&](const core::Pocket& p) { return p.id == ahorro->id; });
        check(it != marcado.end() && it->name == remoto.name, "el cambio se aplica");
        check(it != marcado.end() && it->accountOverride == core::Account::Personal,
              "y la cuenta elegida a mano sigue ahi");

        const int colaAntes = repository.pendingOutboxCount();
        repository.setPocketAccount(ahorro->id, std::nullopt);
        marcado = repository.loadPockets();
        it = std::find_if(marcado.begin(), marcado.end(),
                          [&](const core::Pocket& p) { return p.id == ahorro->id; });
        check(it != marcado.end() && !it->accountOverride, "quitar la marca vuelve al tipo");
        checkMinor(repository.pendingOutboxCount(), colaAntes,
                   "marcar la cuenta no encola nada para subir");
    }

    // --- Categorias --------------------------------------------------------
    {
        storage::Database db(path + QStringLiteral(".categorias"));
        storage::Repository repository(db);
        check(repository.loadCategories().empty(), "una base nueva no trae categorias");

        repository.saveCategory({"Luz", core::Account::Negocio, core::CategoryClass::Fija,
                                 core::MovementKind::Gasto});
        repository.saveCategory({"Comida", core::Account::Personal,
                                 core::CategoryClass::General, core::MovementKind::Gasto});
        auto cats = repository.loadCategories();
        check(cats.size() == 2, "vuelven las dos");
        const core::Category* luz = core::findCategory(cats, "Luz");
        check(luz != nullptr && luz->cls == core::CategoryClass::Fija &&
                  luz->account == core::Account::Negocio,
              "con su cuenta y su clase");

        repository.saveCategory({"luz", core::Account::Negocio, core::CategoryClass::General,
                                 core::MovementKind::Gasto});
        cats = repository.loadCategories();
        check(cats.size() == 2, "'luz' reemplaza a 'Luz' en vez de duplicarla");

        repository.removeCategory("COMIDA");
        cats = repository.loadCategories();
        check(cats.size() == 1 && core::findCategory(cats, "Comida") == nullptr,
              "borrar tampoco distingue mayusculas");
    }

    std::printf("\n%s\n", gFailures == 0 ? "Todo pasa." : "HAY FALLAS.");
    return gFailures == 0 ? 0 : 1;
}
