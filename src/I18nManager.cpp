#include "I18nManager.h"

#include <QApplication>
#include <QDir>
#include <QLibraryInfo>

I18nManager& I18nManager::instance()
{
    static I18nManager inst;
    return inst;
}

I18nManager::I18nManager()
    : m_currentLanguage(Chinese)
{
}

I18nManager::~I18nManager()
{
}

void I18nManager::setLanguage(Language lang)
{
    if (m_currentLanguage == lang && m_currentLanguage != Chinese) return;
    // Always allow first load (Chinese is default but translation may not be loaded yet)

    m_currentLanguage = lang;
    loadTranslation(lang);
    emit languageChanged(lang);
}

void I18nManager::loadTranslation(Language lang)
{
    removeCurrentTranslator();

    QString qmFile;
    switch (lang)
    {
    case Chinese:
        qmFile = ":/translations/ModelViewer_zh_CN.qm";
        break;
    case English:
        qmFile = ":/translations/ModelViewer_en.qm";
        break;
    }

    if (m_translator.load(qmFile))
    {
        qApp->installTranslator(&m_translator);
    }
}

void I18nManager::removeCurrentTranslator()
{
    qApp->removeTranslator(&m_translator);
}

QString I18nManager::languageName(Language lang) const
{
    switch (lang)
    {
    case Chinese:  return QString::fromUtf8("中文");
    case English:  return QString("English");
    default:       return QString("Unknown");
    }
}

QString I18nManager::currentLanguageName() const
{
    return languageName(m_currentLanguage);
}

I18nManager::Language I18nManager::systemLanguage()
{
    QLocale locale = QLocale::system();
    QString lang = locale.name();
    if (lang.startsWith("zh"))
    {
        return Chinese;
    }
    return English;
}
