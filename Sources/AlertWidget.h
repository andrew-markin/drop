#ifndef ALERTWIDGET_H
#define ALERTWIDGET_H

#include <QWidget>

class QLabel;

// A block tinted with the error color, with a message, hidden while there is none. It stands out by
// color rather than by place, so it can sit where it does not shift the controls.
class AlertWidget : public QWidget
{
    Q_OBJECT

public:
    AlertWidget(QWidget* parent = nullptr);

    void setText(const QString& text);

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updateTextColor();

private:
    QLabel* m_textLabel;
};

#endif // ALERTWIDGET_H
