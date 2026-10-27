// Files the computer received in this drop before the page was loaded: from an earlier load of
// the page, or from other devices. See "Status" in docs/PROTOCOL.md.

export function readEarlier({ chunk, files }) {
  return files.map(({ name, size, received, complete }) => ({
    name,
    size,
    sent: complete ? size : Math.min(size, received * chunk),
    complete
  }))
}

// Picked again on this page, they show with the files being sent instead
export function withoutPicked(earlier, items) {
  return earlier.filter(
    (file) => !items.some((item) => item.name === file.name && item.size === file.size)
  )
}
