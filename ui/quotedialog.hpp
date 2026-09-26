#pragma once
//
// ui/quotedialog.hpp — los documentos de Cotizaciones que esperan una
// decision.
//
// Un documento espera cuando importarlo podria contar algo dos veces: parece
// repetido, o ya hay un ingreso anotado a mano que parece el mismo. Cada fila
// trae la decision sugerida ya elegida; Enter las aplica todas.
//
#include <QDialog>

#include "dake/core/quotes.hpp"
#include "snapshot.hpp"

class QComboBox;
class QTableWidget;

namespace dake::ui {

class QuoteReviewDialog : public QDialog {
    Q_OBJECT

public:
    QuoteReviewDialog(const Snapshot& snapshot, QWidget* parent = nullptr);

    /// Documento -> "nuevo", "ignorar" o "enlace:<id>". Solo los que se
    /// decidieron: "esperar" no se guarda.
    [[nodiscard]] core::QuoteDecisions decisions() const;

private:
    struct Row {
        std::string docId;
        QComboBox* choice = nullptr;
    };
    std::vector<Row> rows_;
};

} // namespace dake::ui
