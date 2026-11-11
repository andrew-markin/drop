#include "Translator.h"

#include <QHash>
#include <QStringList>

namespace {

// Plural forms go in the order of the language rules: English one and other, Russian one, few and many
const QHash<QString, QStringList>& getEnglishTranslations()
{
    static const QHash<QString, QStringList> translations = {
        {"Received %n file(s). Click to open the folder.",
         {"Received %n file. Click to open the folder.", "Received %n files. Click to open the folder."}},
    };

    return translations;
}

const QHash<QString, QStringList>& getRussianTranslations()
{
    static const QHash<QString, QStringList> translations = {
        {"Copy Link", {"Копировать ссылку"}},
        {"Copied", {"Скопировано"}},
        {"Send the link to a device in a messenger", {"Отправьте ссылку на устройство в мессенджере"}},
        {"Show QR", {"Показать QR"}},
        {"Scan the QR code with a phone", {"Отсканируйте QR-код телефоном"}},
        {"QR Code", {"QR-код"}},
        {"Name", {"Имя"}},
        {"Size", {"Размер"}},
        {"Status", {"Состояние"}},
        {"Done", {"Готово"}},
        {"B", {"Б"}},
        {"KB", {"КБ"}},
        {"MB", {"МБ"}},
        {"GB", {"ГБ"}},
        {"TB", {"ТБ"}},
        {"Open Folder", {"Открыть папку"}},
        {"Open", {"Открыть"}},
        {"Network: %1", {"Сеть: %1"}},
        {"Scan with a phone on the same network", {"Отсканируйте телефоном в той же сети"}},
        {"This computer is not connected to a local network. "
         "Connect it to the same Wi-Fi or wired network as the devices.",
         {"Компьютер не подключён к локальной сети. "
          "Подключите его к той же сети Wi-Fi или проводной сети, что и устройства."}},
        {"Unable to start the server: %1", {"Не удалось запустить сервер: %1"}},
        {"Unable to initialize the cryptography library.", {"Не удалось инициализировать библиотеку криптографии."}},
        {"Drop is complete", {"Дроп завершён"}},
        {"Received %n file(s). Click to open the folder.",
         {"Получен %n файл. Нажмите, чтобы открыть папку.", "Получено %n файла. Нажмите, чтобы открыть папку.",
          "Получено %n файлов. Нажмите, чтобы открыть папку."}},
    };

    return translations;
}

int getEnglishPluralForm(int n)
{
    return n == 1 ? 0 : 1;
}

int getRussianPluralForm(int n)
{
    if (n % 10 == 1 && n % 100 != 11)
    {
        return 0;
    }

    if (n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 12 || n % 100 > 14))
    {
        return 1;
    }

    return 2;
}

} // namespace

Translator::Translator(const QLocale& locale, QObject* parent) : QTranslator(parent), m_language(locale.language())
{
}

bool Translator::isEmpty() const
{
    return false;
}

QString Translator::translate(const char* context, const char* sourceText, const char* disambiguation, int n) const
{
    Q_UNUSED(context);
    Q_UNUSED(disambiguation);

    bool russian = m_language == QLocale::Russian;
    const QHash<QString, QStringList>& translations = russian ? getRussianTranslations() : getEnglishTranslations();
    QStringList forms = translations.value(QString::fromUtf8(sourceText));

    if (forms.isEmpty())
    {
        return QString(); // Untranslated, the source text is used
    }

    int form = n < 0 ? 0 : (russian ? getRussianPluralForm(n) : getEnglishPluralForm(n));
    return forms.value(form, forms.constFirst());
}
