#pragma once

#include <QObject>
#include <QTranslator>
#include <QString>
#include <QLocale>

class I18nManager : public QObject
{
    Q_OBJECT

public:
    enum Language
    {
        Chinese = 0,
        English = 1
    };

    static I18nManager& instance();

    void setLanguage(Language lang);
    Language currentLanguage() const { return m_currentLanguage; }

    QString languageName(Language lang) const;
    QString currentLanguageName() const;

    static Language systemLanguage();

signals:
    void languageChanged(Language newLanguage);

private:
    I18nManager();
    ~I18nManager() override;

    void loadTranslation(Language lang);
    void removeCurrentTranslator();

    QTranslator m_translator;
    Language m_currentLanguage;
};
