#pragma once
//
// dake/storage/database.hpp — la base SQLite del banco de pruebas.
//
// Unica capa que conoce SQL. La ruta por defecto NO es la de la aplicacion
// real: este programa es un banco de pruebas y no puede tocar, ni por un bug,
// la base con los movimientos de verdad.
//
#include <QString>
#include <memory>
#include <stdexcept>

class QSqlDatabase;

namespace dake::storage {

class StorageError : public std::runtime_error {
public:
    explicit StorageError(const QString& message);
};

class Database {
public:
    /// %APPDATA%\DakeLabs\<nombre de la app>\pruebas.db
    ///
    /// La variable de entorno DAKE_TEST_DB_PATH la reemplaza. Tiene que ser
    /// esa variable y no APPDATA: QStandardPaths resuelve las carpetas del
    /// usuario con la API de Windows y no la lee, asi que redirigir APPDATA no
    /// protege nada.
    [[nodiscard]] static QString defaultPath();

    explicit Database(const QString& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    [[nodiscard]] QSqlDatabase& handle() noexcept;
    [[nodiscard]] QString path() const;

private:
    void applyMigrations();

    QString connectionName_;
    std::unique_ptr<QSqlDatabase> db_;
};

} // namespace dake::storage
