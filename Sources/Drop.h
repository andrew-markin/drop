#ifndef DROP_H
#define DROP_H

#include <QWidget>

class QLabel;

class Drop : public QWidget
{
    Q_OBJECT

public:
    Drop(QWidget* parent = nullptr);

private:
    QLabel* m_urlLabel;
};

#endif // DROP_H
