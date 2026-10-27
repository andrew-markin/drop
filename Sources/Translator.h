#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QLocale>
#include <QTranslator>

// Translations of the window, kept in code: there are few strings, and the Qt builds the packages
// are made with have no Linguist tools. English needs entries only for plural forms.
class Translator : public QTranslator
{
    Q_OBJECT

public:
    explicit Translator(const QLocale& locale, QObject* parent = nullptr);

    bool isEmpty() const override;
    QString translate(const char* context, const char* sourceText, const char* disambiguation = nullptr,
                      int n = -1) const override;

private:
    QLocale::Language m_language;
};

#endif // TRANSLATOR_H
