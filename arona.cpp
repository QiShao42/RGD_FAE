#include "arona.h"
#include "./ui_arona.h"
#include "functionpage.h"
#include <QPixmap>
#include <QPalette>
#include <QResizeEvent>
#include <QShowEvent>

arona::arona(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::arona)
    , functionPage(nullptr)
    , backgroundImageInitialized(false)
{
    ui->setupUi(this);

    // 设置窗口最小尺寸
    this->setMinimumSize(960, 540);

    // 加载背景图片（但不立即设置）
    setBackgroundImage();

    // 创建功能页面并添加到 StackedWidget
    functionPage = new FunctionPage(this);
    ui->stackedWidget->addWidget(functionPage);

    // 连接信号槽
    connect(ui->startButton, &QPushButton::clicked, this, &arona::onStartButtonClicked);
    connect(functionPage, &FunctionPage::backToMainPage, this, &arona::showMainPage);
}

void arona::setBackgroundImage()
{
    // 加载原始背景图片
    originalBackground = QPixmap(":/images/images/back_1.png");
}

void arona::updateBackgroundImage()
{
    if (originalBackground.isNull() || !ui->mainPage) {
        return;
    }

    // 根据当前窗口大小缩放背景图
    QPixmap scaledBackground = originalBackground.scaled(
        ui->mainPage->size(),
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation);

    // 创建调色板并设置背景图
    QPalette palette;
    palette.setBrush(QPalette::Window, QBrush(scaledBackground));

    // 应用到主页面
    ui->mainPage->setAutoFillBackground(true);
    ui->mainPage->setPalette(palette);
}

void arona::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // 窗口大小改变时更新背景图
    updateBackgroundImage();
}

void arona::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 首次显示时设置背景图
    if (!backgroundImageInitialized && !originalBackground.isNull()) {
        updateBackgroundImage();
        backgroundImageInitialized = true;
    }
}

void arona::onStartButtonClicked()
{
    // 切换到功能页面 (索引1)
    ui->stackedWidget->setCurrentIndex(1);
}

void arona::showMainPage()
{
    // 切换回主页面 (索引0)
    ui->stackedWidget->setCurrentIndex(0);

    // 切换回主页面后，强制更新背景图以适应可能改变的窗口大小
    updateBackgroundImage();
}

arona::~arona()
{
    // functionPage 会由 stackedWidget 自动管理和删除
    delete ui;
}
