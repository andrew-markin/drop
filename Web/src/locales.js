// UI translations keyed by language code (see i18n.js). Messages with numbers are functions,
// sizes come already formatted.

const englishPlural = new Intl.PluralRules('en')
const russianPlural = new Intl.PluralRules('ru')

export default {
  en: {
    linkIncomplete: 'The link is incomplete. Scan the QR code shown by Drop on the computer again.',
    chooseFiles: 'Choose Files',
    preparing:
      'The phone is preparing the files. With many photos or videos it can take a couple of minutes.',
    outOfSpace: 'The computer is out of disk space. Free some space and reload this page.',
    connectionLost:
      'The connection is lost, trying again. Check that this device is on the same network as the computer.',
    filesSent: (done, total) =>
      `${done} of ${total} ${englishPlural.select(total) === 'one' ? 'file' : 'files'} sent`,
    sizeOf: (sent, total) => `${sent} of ${total}`,
    keepOpen: 'Keep this page open until all files are sent.',
    alreadyOnComputer: 'Already on the computer',
    pickAgain: (sent, total) => `${sent} of ${total}, pick it again to continue`,
    emptyHint: 'Photos, videos and any other files go to the Downloads folder of the computer.',
    errorNoSpace: 'The computer is out of disk space',
    errorWrongLink: 'The link is wrong, scan the QR code again',
    errorRestarted: 'Drop on the computer was restarted, scan the QR code again',
    errorOther: 'Not sent, something went wrong'
  },
  ru: {
    linkIncomplete: 'Ссылка неполная. Отсканируйте QR-код в Drop на компьютере ещё раз.',
    chooseFiles: 'Выбрать файлы',
    preparing: 'Телефон готовит файлы. Если фото и видео много, это может занять пару минут.',
    outOfSpace: 'На компьютере закончилось место. Освободите место и перезагрузите страницу.',
    connectionLost:
      'Связь потеряна, пробуем снова. Проверьте, что устройство в той же сети, что и компьютер.',
    // "Из" takes the genitive: из 1 файла, из 2 файлов, из 21 файла
    filesSent: (done, total) =>
      `Отправлено ${done} из ${total} ${russianPlural.select(total) === 'one' ? 'файла' : 'файлов'}`,
    sizeOf: (sent, total) => `${sent} из ${total}`,
    keepOpen: 'Не закрывайте страницу, пока не отправятся все файлы.',
    alreadyOnComputer: 'Уже на компьютере',
    pickAgain: (sent, total) => `${sent} из ${total}, выберите файл снова, чтобы продолжить`,
    emptyHint: 'Фото, видео и любые другие файлы попадут в папку «Загрузки» на компьютере.',
    errorNoSpace: 'На компьютере закончилось место',
    errorWrongLink: 'Неверная ссылка, отсканируйте QR-код ещё раз',
    errorRestarted: 'Drop на компьютере перезапущен, отсканируйте QR-код ещё раз',
    errorOther: 'Не отправлен, что-то пошло не так'
  }
}
