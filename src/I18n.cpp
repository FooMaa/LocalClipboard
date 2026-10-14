#include "I18n.h"
#include "Profile.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace {
const QHash<QString, QString> &russian() {
    static const QHash<QString, QString> entries = {
#include "Russian.inc"
    };
    return entries;
}
class Translator final : public QTranslator {
public:
    bool isEmpty() const override { return false; }
    QString translate(const char *, const char *source, const char *, int) const override {
        return I18n::language() == "ru" ? russian().value(QString::fromUtf8(source)) : QString();
    }
};
Translator &translator() { static Translator value; return value; }
QString preferences;
QString preferenceFile() { return preferences.isEmpty() ? QDir(Profile::dataDirectory()).filePath("preferences.ini") : preferences; }
}
I18n &I18n::instance() { static I18n manager; return manager; }
QString I18n::language() { return instance().m_language; }
void I18n::initialize(const QString &requested, const QString &preferenceDirectory) {
    const QString previous = language();
    preferences = preferenceDirectory.isEmpty() ? QString() : QDir(preferenceDirectory).filePath("preferences.ini");
    const QString fallback = QLocale::system().language() == QLocale::Russian ? "ru" : "en";
    const QString saved = QSettings(preferenceFile(), QSettings::IniFormat).value("language", fallback).toString();
    instance().m_language = saved == "ru" ? "ru" : "en";
    QCoreApplication::removeTranslator(&translator());
    QCoreApplication::installTranslator(&translator());
    if (requested == "ru" || requested == "en") setLanguage(requested);
    if (previous != language()) emit instance().languageChanged();
}
void I18n::setLanguage(const QString &code) {
    if (code != "ru" && code != "en") return;
    QDir().mkpath(QFileInfo(preferenceFile()).absolutePath());
    QSettings settings(preferenceFile(), QSettings::IniFormat); settings.setValue("language", code); settings.sync();
    if (instance().m_language == code) return;
    instance().m_language = code;
    QCoreApplication::removeTranslator(&translator());
    QCoreApplication::installTranslator(&translator());
    emit instance().languageChanged();
}
QString I18n::text(const QString &source) {
    if (language() != "ru" || source.isEmpty()) return source;
    const auto &entries = russian();
    const auto found = entries.constFind(source);
    if (found != entries.cend()) return found.value();
    // Dynamic transport / OS error details follow a fixed, translated prefix.
    for (auto it = entries.cbegin(); it != entries.cend(); ++it)
        if (it.key().endsWith(' ') && source.startsWith(it.key())) return it.value() + text(source.mid(it.key().size()));
    return source;
}
