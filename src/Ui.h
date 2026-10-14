#pragma once
#include <QDialog>
#include <QVBoxLayout>
class QLabel;
class QPushButton;
class QComboBox;
class QCheckBox;
namespace Ui {
void initialize();
QLabel *label(const char *text, QWidget *parent, const char *role = nullptr);
QPushButton *button(const char *text, QWidget *parent, bool primary = false);
QCheckBox *checkBox(const char *text, QWidget *parent);
QComboBox *languagePicker(QWidget *parent);
void title(QWidget *widget, const char *text);
struct Form { QVBoxLayout *body; QVBoxLayout *footer; };
Form form(QDialog *dialog, const char *heading, const char *description);
void field(QVBoxLayout *layout, const char *name, QWidget *input);
}
