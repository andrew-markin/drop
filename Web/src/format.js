// Bytes read awkwardly in most languages ("1 byte", "999 byte"), small sizes go in kilobytes
const units = ['kilobyte', 'megabyte', 'gigabyte', 'terabyte']

export function formatSize(size, locale = undefined) {
  // A file that is not empty never shows as zero
  let value = size > 0 ? Math.max(size / 1000, 0.1) : 0
  let unit = 0
  while (value >= 1000 && unit < units.length - 1) {
    value /= 1000
    unit++
  }
  return new Intl.NumberFormat(locale, {
    style: 'unit',
    unit: units[unit],
    unitDisplay: 'short',
    maximumFractionDigits: value >= 100 ? 0 : 1
  }).format(value)
}
