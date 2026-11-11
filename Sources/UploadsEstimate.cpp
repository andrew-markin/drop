#include "UploadsEstimate.h"

#include <QtMath>

namespace {

static const qint64 Minute = 60;
static const qint64 Hour = 60 * Minute;

// Up to the next step, so the drop ends rather before than after the time told
qint64 roundUp(qint64 value, qint64 step)
{
    return (value + step - 1) / step * step;
}

} // namespace

UploadsEstimate::UploadsEstimate(QObject* parent) : QObject(parent)
{
    // Nothing
}

void UploadsEstimate::update(qint64 now, qint64 totalSize, qint64 receivedSize)
{
    if (receivedSize >= totalSize)
    {
        // Everything is here, the next files make a new drop
        m_baseTotalSize = totalSize;
        m_baseReceivedSize = receivedSize;
        m_samples.clear();
        m_progressTime = -1;
        m_shownTime = -1;
        m_shownAt = -1;
    }
    else if (m_progressTime < 0 || receivedSize != m_receivedSize)
    {
        m_progressTime = now;
    }

    m_totalSize = totalSize;
    m_receivedSize = receivedSize;

    if (!isActive())
    {
        return;
    }

    m_samples.append({now, receivedSize});

    while (m_samples.size() > 2 && now - m_samples.at(1).time >= SpeedWindow)
    {
        m_samples.removeFirst();
    }

    const Sample& first = m_samples.constFirst();
    qint64 span = now - first.time;
    qint64 received = receivedSize - first.receivedSize;

    if (now - m_progressTime >= StallTimeout || span < MinimumSpan || received <= 0)
    {
        // Nothing to go by, or nothing comes: the time left would only mislead
        if (now - m_progressTime >= StallTimeout)
        {
            m_shownTime = -1;
        }

        return;
    }

    qint64 remaining = qCeil(double(totalSize - receivedSize) * span / received / 1000);

    bool due = m_shownAt < 0 || now - m_shownAt >= ShowInterval;
    bool changed = m_shownTime < 0 || (describe(remaining) != describe(m_shownTime) &&
                                       qAbs(remaining - m_shownTime) > m_shownTime * ShowChange);

    if (due && changed)
    {
        m_shownTime = remaining;
        m_shownAt = now;
    }
}

bool UploadsEstimate::isActive() const
{
    return m_receivedSize < m_totalSize;
}

int UploadsEstimate::percent() const
{
    qint64 total = m_totalSize - m_baseTotalSize;
    qint64 received = m_receivedSize - m_baseReceivedSize;

    // Never 100 before the end, which shows no percent at all
    return total > 0 ? int(qMin<qint64>(99, received * 100 / total)) : 0;
}

qint64 UploadsEstimate::remainingTime() const
{
    return isActive() ? m_shownTime : -1;
}

QString UploadsEstimate::remainingText() const
{
    qint64 seconds = remainingTime();
    return seconds < 0 ? QString() : describe(seconds);
}

QString UploadsEstimate::describe(qint64 seconds)
{
    if (seconds < Minute)
    {
        return tr("less than a minute left");
    }

    if (seconds < 10 * Minute)
    {
        return tr("%n min left", "", int(roundUp(seconds, Minute) / Minute));
    }

    if (seconds < Hour)
    {
        qint64 minutes = roundUp(seconds, 5 * Minute) / Minute;

        if (minutes < 60)
        {
            return tr("%n min left", "", int(minutes));
        }

        seconds = Hour; // Rounded up to an hour
    }

    qint64 minutes = roundUp(seconds, 10 * Minute) / Minute;

    if (minutes % 60 == 0)
    {
        return tr("%n h left", "", int(minutes / 60));
    }

    return tr("%1 h %2 min left").arg(minutes / 60).arg(minutes % 60);
}
