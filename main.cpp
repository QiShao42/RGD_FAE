#include "arona.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置应用程序图标（显示在任务栏、Alt+Tab切换窗口等处）
    a.setWindowIcon(QIcon(":/icons/icons/app_icon.png"));

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "RGD_FAE_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    arona w;
    w.show();
    return a.exec();
}
