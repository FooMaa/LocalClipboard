#include "Ui.h"
#include "I18n.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QHBoxLayout>
#include <QStyle>
#include <QListWidget>
#include <QLineEdit>
#include <QInputMethod>
#include <QScroller>
#include <QTimer>
#include <QPointer>
#include <QKeyEvent>
#include <QScrollerProperties>
#ifdef Q_OS_ANDROID
#include "AndroidUi.h"
#endif

namespace {
class TouchComboBox final : public QComboBox {
public:
    using QComboBox::QComboBox;
    void showPopup() override {
        if (!Ui::touchMode()) { QComboBox::showPopup(); return; }
        qApp->inputMethod()->hide();
#ifdef Q_OS_ANDROID
        QStringList items;
        QList<bool> enabled;
        for (int i = 0; i < count(); ++i) {
            items.append(itemText(i));
            enabled.append(model()->flags(model()->index(i, modelColumn())) & Qt::ItemIsEnabled);
        }
        const QPointer<TouchComboBox> self(this);
        const int selected = AndroidUi::choose(this, accessibleName(), items, enabled, currentIndex());
        if (!self) return;
        if (selected >= 0) setCurrentIndex(selected);
        QComboBox::hidePopup();
        return;
#endif
        QDialog dialog(window()); dialog.setObjectName("touchChoices");
        Ui::prepareWindow(&dialog); dialog.setWindowTitle(accessibleName());
        auto *layout = new QVBoxLayout(&dialog);
        auto *choices = new QListWidget(&dialog); choices->setObjectName("touchChoiceList");
        choices->setWordWrap(true); choices->setSpacing(6);
        for (int index = 0; index < count(); ++index) {
            auto *item = new QListWidgetItem(itemText(index), choices);
            item->setSizeHint(QSize(0, qMax(56, fontMetrics().height() * 2 + 16)));
            if (!(model()->flags(model()->index(index, modelColumn())) & Qt::ItemIsEnabled))
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
        choices->setCurrentRow(currentIndex()); layout->addWidget(choices, 1);
        auto *cancel = Ui::button("Cancel", &dialog); layout->addWidget(cancel);
        QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
        QObject::connect(choices, &QListWidget::itemClicked, &dialog, [&](QListWidgetItem *item) {
            setCurrentIndex(choices->row(item)); dialog.accept();
        });
        dialog.exec();
        QComboBox::hidePopup();
    }
};
class WrappingButton final : public QPushButton {
public:
    WrappingButton(const char *source, QWidget *parent, bool primary) : QPushButton(parent) {
        setProperty("primary", primary);
        if (Ui::touchMode()) setAutoDefault(false);
        m_label = Ui::label(source, this); m_label->setAlignment(Qt::AlignCenter); m_label->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *row = new QHBoxLayout(this); row->setContentsMargins(14, 10, 14, 10); row->addWidget(m_label);
        QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred); policy.setHeightForWidth(true); setSizePolicy(policy);
        const QString key = QString::fromUtf8(source); setAccessibleName(I18n::text(key));
        QObject::connect(&I18n::instance(), &I18n::languageChanged, this, [this, key] { setAccessibleName(I18n::text(key)); });
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return qMax(QPushButton::sizeHint().height(), m_label->heightForWidth(qMax(1, width - 28)) + 20); }
    QSize sizeHint() const override { return {m_label->sizeHint().width() + 28, heightForWidth(m_label->sizeHint().width() + 28)}; }
    QSize minimumSizeHint() const override { return {68, QPushButton::sizeHint().height()}; }
private:
    QLabel *m_label;
};
class WrappingCheckBox final : public QCheckBox {
public:
    WrappingCheckBox(const char *source, QWidget *parent) : QCheckBox(parent) {
        m_label = Ui::label(source, this); m_label->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_indent = style()->pixelMetric(QStyle::PM_IndicatorWidth) + 10;
        auto *row = new QHBoxLayout(this); row->setContentsMargins(m_indent, 5, 0, 5); row->addWidget(m_label);
        QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred); policy.setHeightForWidth(true); setSizePolicy(policy);
        const QString key = QString::fromUtf8(source);
        setAccessibleName(I18n::text(key));
        QObject::connect(&I18n::instance(), &I18n::languageChanged, this, [this, key] { setAccessibleName(I18n::text(key)); });
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override {
        return qMax(QCheckBox::sizeHint().height(), m_label->heightForWidth(qMax(1, width - m_indent)) + 10);
    }
    QSize sizeHint() const override { return {m_label->sizeHint().width() + m_indent, heightForWidth(m_label->sizeHint().width() + m_indent)}; }
    QSize minimumSizeHint() const override { return {m_indent + 40, QCheckBox::sizeHint().height()}; }
protected:
    bool hitButton(const QPoint &point) const override { return rect().contains(point); }
private:
    QLabel *m_label;
    int m_indent;
};
template <typename Widget> void bind(Widget *widget, const char *source) {
    const QString key = QString::fromUtf8(source);
    widget->setText(I18n::text(key));
    if (key.isEmpty()) return;
    QObject::connect(&I18n::instance(), &I18n::languageChanged, widget, [widget, key] { widget->setText(I18n::text(key)); });
}
}
bool Ui::touchMode() {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return true;
#else
    return qApp->property("touchUi").toBool();
#endif
}
void Ui::prepareWindow(QWidget *widget) {
    if (!touchMode()) return;
    widget->setWindowFlags((widget->windowFlags() & ~Qt::WindowType_Mask) | Qt::Window);
    widget->setWindowState(Qt::WindowMaximized);
}
void Ui::showWindow(QWidget *widget) {
    if (touchMode()) widget->showMaximized(); else widget->show();
}
void Ui::enableTouchScroll(QScrollArea *scroll) {
    if (!touchMode()) return;
    QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
    auto *scroller = QScroller::scroller(scroll->viewport());
    auto properties = scroller->scrollerProperties();
    properties.setScrollMetric(QScrollerProperties::FrameRate, QScrollerProperties::Fps60);
    properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.003);
    properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    scroller->setScrollerProperties(properties);
    // Bring the focused editor back into view after Android resizes for the IME.
    QObject::connect(qApp, &QApplication::focusChanged, scroll, [scroll](QWidget *, QWidget *focused) {
        if (!qobject_cast<QLineEdit *>(focused) || !scroll->isAncestorOf(focused)) return;
        QPointer<QWidget> target(focused);
        QTimer::singleShot(250, scroll, [scroll, target] {
            if (target && target->hasFocus() && QScroller::scroller(scroll->viewport())->state() == QScroller::Inactive)
                scroll->ensureWidgetVisible(target, 12, 24);
        });
    });
    QObject::connect(qApp->inputMethod(), &QInputMethod::keyboardRectangleChanged, scroll, [scroll] {
        QPointer<QWidget> focused(qApp->focusWidget());
        if (qobject_cast<QLineEdit *>(focused.data()) && scroll->isAncestorOf(focused))
            QTimer::singleShot(0, scroll, [scroll, focused] {
                if (focused && focused->hasFocus() && QScroller::scroller(scroll->viewport())->state() == QScroller::Inactive)
                    scroll->ensureWidgetVisible(focused, 12, 24);
            });
    });
}
QComboBox *Ui::comboBox(QWidget *parent) {
    auto *combo = new TouchComboBox(parent);
    if (touchMode()) combo->setMinimumHeight(48);
    return combo;
}
void Ui::initialize() {
    QApplication::setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f4f6fa"));
    palette.setColor(QPalette::WindowText, QColor("#1e293b"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor("#f4f6fa"));
    palette.setColor(QPalette::Text, QColor("#1e293b"));
    palette.setColor(QPalette::Button, Qt::white);
    palette.setColor(QPalette::ButtonText, QColor("#1e293b"));
    palette.setColor(QPalette::Highlight, QColor("#2563eb"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#94a3b8"));
    QApplication::setPalette(palette);
    qApp->setStyleSheet(R"(
        QLineEdit, QComboBox, QListWidget { background: white; border: 1px solid #cbd5e1; border-radius: 8px; padding: 8px; }
        QLineEdit:focus, QComboBox:focus { border-color: #2563eb; }
        QPushButton { background: white; border: 1px solid #cbd5e1; border-radius: 8px; padding: 10px 14px; }
        QPushButton:hover { background: #eff6ff; border-color: #93b4f5; }
        QPushButton:pressed { background: #dbeafe; }
        QPushButton[primary="true"] { background: #2563eb; border-color: #2563eb; color: white; font-weight: 600; }
        QPushButton[primary="true"]:hover { background: #1d4ed8; }
        QPushButton QLabel { color: #1e293b; background: transparent; }
        QPushButton[primary="true"] QLabel { color: white; font-weight: 600; }
        QPushButton:disabled { color: #94a3b8; background: #e2e8f0; }
        QPushButton:disabled QLabel { color: #94a3b8; }
        QLabel[role="heading"] { font-size: 22pt; font-weight: 600; }
        QLabel[role="section"] { font-weight: 600; color: #334155; }
        QLabel[role="muted"] { color: #64748b; }
        QLabel[role="error"] { color: #b91c1c; }
        QScrollArea { border: none; background: transparent; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 4px 1px; }
        QScrollBar::handle:vertical { background: #cbd5e1; border-radius: 3px; min-height: 28px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QCheckBox { spacing: 8px; padding: 5px 0; }
    )");
}
QLabel *Ui::label(const char *text, QWidget *parent, const char *role) {
    auto *result = new QLabel(parent);
    result->setWordWrap(true); result->setTextFormat(Qt::PlainText);
    // A paragraph must not impose its unwrapped text as the minimum width.
    result->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    if (role) result->setProperty("role", role);
    bind(result, text); return result;
}
QPushButton *Ui::button(const char *text, QWidget *parent, bool primary) {
    return new WrappingButton(text, parent, primary);
}
QCheckBox *Ui::checkBox(const char *text, QWidget *parent) {
    return new WrappingCheckBox(text, parent);
}
QComboBox *Ui::languagePicker(QWidget *parent) {
    auto *picker = comboBox(parent); picker->setObjectName("languagePicker");
    picker->setAccessibleName(I18n::text("Language"));
    picker->addItem(QString::fromUtf8("Русский"), "ru"); picker->addItem("English", "en");
    picker->setCurrentIndex(I18n::language() == "ru" ? 0 : 1);
    QObject::connect(picker, &QComboBox::currentIndexChanged, picker, [picker](int index) { I18n::setLanguage(picker->itemData(index).toString()); });
    QObject::connect(&I18n::instance(), &I18n::languageChanged, picker, [picker] {
        picker->setAccessibleName(I18n::text("Language"));
        const QSignalBlocker blocker(picker); picker->setCurrentIndex(I18n::language() == "ru" ? 0 : 1);
    });
    return picker;
}
void Ui::title(QWidget *widget, const char *text) {
    const QString key = QString::fromUtf8(text); widget->setWindowTitle(I18n::text(key));
    QObject::connect(&I18n::instance(), &I18n::languageChanged, widget, [widget, key] { widget->setWindowTitle(I18n::text(key)); });
}
Ui::Form Ui::form(QDialog *dialog, const char *heading, const char *description) {
    prepareWindow(dialog);
    auto *outer = new QVBoxLayout(dialog); outer->setContentsMargins(0, 0, 0, 16); outer->setSpacing(0);
    auto *scroll = new QScrollArea(dialog); scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scroll);
    auto *body = new QVBoxLayout(content); body->setContentsMargins(24, 24, 24, 24); body->setSpacing(12);
    body->addWidget(languagePicker(content), 0, Qt::AlignRight);
    body->addWidget(label(heading, content, "heading")); body->addWidget(label(description, content, "muted"));
    body->setSizeConstraint(QLayout::SetMinimumSize);
    scroll->setWidget(content); enableTouchScroll(scroll); outer->addWidget(scroll, 1);
    auto *footer = new QVBoxLayout; footer->setContentsMargins(24, 12, 24, 0); outer->addLayout(footer);
    const QSize available = dialog->screen()->availableGeometry().size();
    if (!touchMode()) dialog->resize(qMin(520, available.width()), qMin(660, available.height()));
    return {body, footer};
}
void Ui::field(QVBoxLayout *layout, const char *name, QWidget *input) {
    auto *caption = label(name, input->parentWidget(), "section"); caption->setBuddy(input);
    layout->addWidget(caption); layout->addWidget(input); input->setMinimumWidth(0);
    input->setAccessibleName(caption->text());
    QObject::connect(&I18n::instance(), &I18n::languageChanged, input, [input, caption] { input->setAccessibleName(caption->text()); });
    if (touchMode()) {
        input->setMinimumHeight(48);
        if (auto *editor = qobject_cast<QLineEdit *>(input)) {
            editor->setClearButtonEnabled(!editor->isReadOnly());
            editor->setInputMethodHints(Qt::ImhNoPredictiveText);
            QObject::connect(editor, &QLineEdit::returnPressed, editor, [editor] { QKeyEvent next(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier); QApplication::sendEvent(editor, &next); });
        }
    }
}
