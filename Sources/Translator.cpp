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
        {"%1 of %n file(s) received", {"%1 of %n file received", "%1 of %n files received"}},
    };

    return translations;
}

const QHash<QString, QStringList>& getRussianTranslations()
{
    static const QHash<QString, QStringList> translations = {
        {"Copy Link", {"Копировать ссылку"}},
        {"Copied", {"Скопировано"}},
        {"Send the link to a device in a messenger, or scan the QR code with it",
         {"Отправьте ссылку на устройство в мессенджере или отсканируйте им QR-код"}},
        {"Name", {"Имя"}},
        {"Size", {"Размер"}},
        {"Status", {"Состояние"}},
        {"Done", {"Готово"}},
        {"Open Folder", {"Открыть папку"}},
        {"Network:", {"Сеть:"}},
        {"Scan the QR code with a phone connected to the same network.",
         {"Отсканируйте QR-код телефоном, подключённым к той же сети."}},
        {"This computer is not connected to a local network. "
         "Connect it to the same Wi-Fi or wired network as the devices.",
         {"Компьютер не подключён к локальной сети. "
          "Подключите его к той же сети Wi-Fi или проводной сети, что и устройства."}},
        {"Unable to start the server: %1", {"Не удалось запустить сервер: %1"}},
        {"Unable to initialize the cryptography library.", {"Не удалось инициализировать библиотеку криптографии."}},
        {"The page does not open on the phone? Make sure the network is marked as private in "
         "Windows Settings, Network & internet: Windows Firewall lets devices connect to Drop "
         "on private networks only.",
         {"Страница не открывается на телефоне? Проверьте, что сеть отмечена как частная в параметрах Windows, "
          "раздел «Сеть и Интернет»: брандмауэр Windows пускает устройства к Drop только в частных сетях."}},
        {"The page does not open on the phone? Make sure it is on the same network as this "
         "computer, and that the firewall allows incoming connections.",
         {"Страница не открывается на телефоне? Проверьте, что он в той же сети, что и компьютер, "
          "и что брандмауэр разрешает входящие подключения."}},
        {"Drop is complete", {"Дроп завершён"}},
        {"Received %n file(s). Click to open the folder.",
         {"Получен %n файл. Нажмите, чтобы открыть папку.", "Получено %n файла. Нажмите, чтобы открыть папку.",
          "Получено %n файлов. Нажмите, чтобы открыть папку."}},
        // "Из" takes the genitive: из 1 файла, из 2 файлов, из 21 файла
        {"%1 of %n file(s) received", {"Получено %1 из %n файла", "Получено %1 из %n файлов", "Получено %1 из %n файлов"}},
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
