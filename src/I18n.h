#pragma once
#include <QObject>
#include <QString>
class I18n final : public QObject {
    Q_OBJECT
public:
    static I18n &instance();
    static void initialize(const QString &requested = {}, const QString &preferenceDirectory = {});
    static QString language();
    static void setLanguage(const QString &code);
    static QString text(const QString &source);
signals:
    void languageChanged();
private:
    QString m_language = "en";
};
