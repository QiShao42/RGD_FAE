#ifndef FUNCTIONPAGE_H
#define FUNCTIONPAGE_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class FunctionPage;
}
QT_END_NAMESPACE

class FunctionPage : public QWidget
{
    Q_OBJECT

public:
    explicit FunctionPage(QWidget *parent = nullptr);
    ~FunctionPage();

signals:
    void backToMainPage();

private slots:
    void onBackButtonClicked();
    void onRxCountChanged(int value);
    void onTxCountChanged(int value);
    void onRawThresholdChanged(int value);
    void onSignalThresholdChanged(int value);

private:
    void initializeTable();
    void updateTableSize();
    void loadConfig();
    void saveConfig();

    Ui::FunctionPage *ui;
};

#endif // FUNCTIONPAGE_H
