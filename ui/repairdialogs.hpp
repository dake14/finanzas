#pragma once
//
// ui/repairdialogs.hpp — alta de una reparacion y su entrega.
//
// Los dos se usan sin mouse. Alta: plantilla, cliente, equipo, Enter; lo
// demas viene de la plantilla (meta: menos de 30 segundos). Entrega: las horas
// reales y el precio final ya vienen puestos; Enter entrega y cobra, que es el
// caso comun.
//
#include <QDialog>
#include <QElapsedTimer>

#include "snapshot.hpp"

class QComboBox;
class QLabel;
class QLineEdit;
class QDateEdit;

namespace dake::ui {

class NewRepairDialog : public QDialog {
    Q_OBJECT

public:
    NewRepairDialog(const Snapshot& snapshot, QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

public:
    [[nodiscard]] core::Id templateId() const;
    [[nodiscard]] QString client() const;
    [[nodiscard]] QString device() const;
    /// Lo que tardo desde que se abrio hasta Crear.
    [[nodiscard]] qint64 elapsedMs() const noexcept { return elapsed_; }

private:
    void tryAccept();
    /// Vuelve a elegir la plantilla con lo escrito y la marca en la lista.
    void matchTemplate();
    void showTemplates();

    std::vector<core::RepairTemplate> templates_;
    int chosen_ = -1;
    QLineEdit* template_ = nullptr;
    QLabel* templateList_ = nullptr;
    QLineEdit* client_ = nullptr;
    QLineEdit* device_ = nullptr;
    QLabel* error_ = nullptr;
    QElapsedTimer clock_;
    qint64 elapsed_ = 0;
};

class DeliverDialog : public QDialog {
    Q_OBJECT

public:
    DeliverDialog(const core::Repair& repair, const core::Money& price, core::Date today,
                  QWidget* parent = nullptr);

    [[nodiscard]] int realMinutes() const;
    [[nodiscard]] std::int64_t priceMinor() const;
    [[nodiscard]] core::Date date() const;
    [[nodiscard]] bool charged() const noexcept { return charged_; }

private:
    void finish(bool charged);

    core::Currency currency_;
    QLineEdit* hours_ = nullptr;
    QLineEdit* price_ = nullptr;
    QDateEdit* date_ = nullptr;
    QLabel* error_ = nullptr;
    bool charged_ = true;
};

} // namespace dake::ui
