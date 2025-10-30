#ifndef ARONA_H
#define ARONA_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class arona;
}
QT_END_NAMESPACE

class arona : public QMainWindow
{
    Q_OBJECT

public:
    arona(QWidget *parent = nullptr);
    ~arona();

private:
    Ui::arona *ui;
};
#endif // ARONA_H
