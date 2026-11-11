#ifndef UPLOADSESTIMATE_H
#define UPLOADSESTIMATE_H

#include <QList>
#include <QObject>

// How far the current drop is and about when it ends. Fed with the totals from time to time, it
// tells the time left roughly and changes it rarely, so it reads at a glance and does not jitter.
class UploadsEstimate : public QObject
{
    Q_OBJECT

public:
    static const qint64 SpeedWindow = 30000; // 30s of progress make the speed
    static const qint64 MinimumSpan = 5000; // 5s, less tells nothing yet
    static const qint64 StallTimeout = 30000; // 30s without a chunk, the device may be gone
    static const qint64 ShowInterval = 5000; // 5s at least between changes of the time shown
    static constexpr double ShowChange = 0.1; // Of the time shown, smaller changes are not shown

    UploadsEstimate(QObject* parent = nullptr);

    // Times in milliseconds from any start, sizes of the files there are to receive
    void update(qint64 now, qint64 totalSize, qint64 receivedSize);

    bool isActive() const;
    int percent() const; // Of the current drop, which starts when everything before is received
    qint64 remainingTime() const; // Seconds as shown, -1 when unknown
    QString remainingText() const; // Empty when unknown

    // Rounded the coarser the longer: "less than a minute", minutes, 5 minutes, 10 minutes
    static QString describe(qint64 seconds);

private:
    struct Sample
    {
        qint64 time;
        qint64 receivedSize;
    };

    qint64 m_baseTotalSize = 0;
    qint64 m_baseReceivedSize = 0;
    qint64 m_totalSize = 0;
    qint64 m_receivedSize = 0;

    QList<Sample> m_samples;
    qint64 m_progressTime = -1;

    qint64 m_shownTime = -1;
    qint64 m_shownAt = -1;
};

#endif // UPLOADSESTIMATE_H
