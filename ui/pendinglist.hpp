#pragma once
//
// ui/pendinglist.hpp — los pendientes, todos a la vista, uno por fila.
//
// Reemplaza a la seccion Revision, que mostraba uno por vez. Cada fila trae la
// accion que la resuelve (confirmar un monto, poner las horas, cuadrar un
// bolsillo…) y "Despues", que la pospone una semana. Resolver una fila no
// mueve las demas: la lista se vuelve a armar desde la bandeja del nucleo, en
// el mismo orden.
//
// No guarda nada: emite señales y la ventana principal escribe.
//
#include <QWidget>

#include <string>

#include "snapshot.hpp"

class QVBoxLayout;

namespace dake::ui {

class PendingList : public QWidget {
    Q_OBJECT

public:
    explicit PendingList(QWidget* parent = nullptr);

    void setSnapshot(const Snapshot& snapshot);

    /// La clave con que se pospone un pendiente: el id, salvo "jobId:horas",
    /// "pocketId:cuadrar" y "cotizaciones".
    [[nodiscard]] static std::string snoozeKey(const core::InboxItem& item);

signals:
    void recurringConfirmed(const dake::core::Id& movementId, qint64 amountMinor);
    void quotesReviewRequested();
    void chargeRequested(const dake::core::Id& jobId);
    void repairOpened(const dake::core::Id& jobId);
    void realHoursSet(const dake::core::Id& jobId, int minutes);
    void partCostSet(const dake::core::Id& partId, qint64 costMinor);
    void reconcileRequested(const dake::core::Id& pocketId);
    void snoozed(const std::string& key, int days);

private:
    [[nodiscard]] QWidget* buildRow(const core::InboxItem& item);

    Snapshot snapshot_;
    QVBoxLayout* rows_ = nullptr;
};

} // namespace dake::ui
