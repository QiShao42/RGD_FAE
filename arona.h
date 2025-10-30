#ifndef ARONA_H
#define ARONA_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class arona;
}
QT_END_NAMESPACE

class FunctionPage;

class arona : public QMainWindow
{
    Q_OBJECT

public:
    arona(QWidget *parent = nullptr);
    ~arona();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onStartButtonClicked();
    void showMainPage();

private:
    void setBackgroundImage();
    void updateBackgroundImage();
    Ui::arona *ui;
    QPixmap originalBackground;
    FunctionPage *functionPage;
    bool backgroundImageInitialized;
};
#endif // ARONA_H
