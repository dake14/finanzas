#include "categorybox.hpp"

#include <QCompleter>
#include <QLineEdit>
#include <QStringListModel>

namespace dake::ui {

CategoryBox::CategoryBox(QWidget* parent) : QComboBox(parent) {
    setEditable(true);
    // Evitar que el texto libre se agregue automaticamente a las opciones
    setInsertPolicy(QComboBox::NoInsert);

    completionModel_ = new QStringListModel(this);
    completer_ = new QCompleter(completionModel_, this);
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setFilterMode(Qt::MatchContains);
    completer_->setCompletionMode(QCompleter::PopupCompletion);

    // QComboBox::setCompleter pisa el completador al cambiar el modelo del
    // combo perdiendo el filtrado por subcadena
    lineEdit()->setCompleter(completer_);
}

void CategoryBox::setSuggestions(const QStringList& suggestions) {
    const QString current = currentText();

    blockSignals(true);
    clear();
    addItems(suggestions);
    setCurrentText(current);
    blockSignals(false);

    completionModel_->setStringList(suggestions);

    // clear() y addItems() en combos editables pueden destruir el completador
    // asignado al lineEdit, asi que lo forzamos de nuevo
    lineEdit()->setCompleter(completer_);
}

QString CategoryBox::category() const {
    return currentText().trimmed();
}

void CategoryBox::setCategory(const QString& text) {
    setCurrentText(text);
}

} // namespace dake::ui
