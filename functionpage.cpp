#include "functionpage.h"
#include "./ui_functionpage.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHeaderView>

FunctionPage::FunctionPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FunctionPage)
{
    ui->setupUi(this);

    // 设置上下区域的 8:2 比例
    ui->contentVerticalLayout->setStretch(0, 8);  // topArea
    ui->contentVerticalLayout->setStretch(1, 0);  // dividerLine
    ui->contentVerticalLayout->setStretch(2, 2);  // bottomArea

    // 加载配置
    loadConfig();

    // 初始化表格
    initializeTable();

    // 连接信号槽
    connect(ui->backButton, &QPushButton::clicked, this, &FunctionPage::onBackButtonClicked);
    connect(ui->rxSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &FunctionPage::onRxCountChanged);
    connect(ui->txSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &FunctionPage::onTxCountChanged);
    connect(ui->rawThresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &FunctionPage::onRawThresholdChanged);
    connect(ui->signalThresholdSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &FunctionPage::onSignalThresholdChanged);
}

FunctionPage::~FunctionPage()
{
    saveConfig();
    delete ui;
}

void FunctionPage::onBackButtonClicked()
{
    emit backToMainPage();
}

void FunctionPage::onRxCountChanged(int value)
{
    updateTableSize();
    saveConfig();
}

void FunctionPage::onTxCountChanged(int value)
{
    updateTableSize();
    saveConfig();
}

void FunctionPage::onRawThresholdChanged(int value)
{
    saveConfig();
}

void FunctionPage::onSignalThresholdChanged(int value)
{
    saveConfig();
}

void FunctionPage::initializeTable()
{
    // 设置表格行列数
    int rxCount = ui->rxSpinBox->value();
    int txCount = ui->txSpinBox->value();

    ui->dataTable->setRowCount(txCount);
    ui->dataTable->setColumnCount(rxCount);

    // 设置行标题 (TX0, TX1, ...)
    QStringList rowHeaders;
    for (int i = 0; i < txCount; ++i) {
        rowHeaders << QString("TX%1").arg(i);
    }
    ui->dataTable->setVerticalHeaderLabels(rowHeaders);

    // 设置列标题 (RX0, RX1, ...)
    QStringList colHeaders;
    for (int i = 0; i < rxCount; ++i) {
        colHeaders << QString("RX%1").arg(i);
    }
    ui->dataTable->setHorizontalHeaderLabels(colHeaders);

    // 设置单元格最小尺寸
    ui->dataTable->verticalHeader()->setMinimumSectionSize(18);
    ui->dataTable->horizontalHeader()->setMinimumSectionSize(34);

    // 设置表头调整模式为自动拉伸
    ui->dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->dataTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void FunctionPage::updateTableSize()
{
    int rxCount = ui->rxSpinBox->value();
    int txCount = ui->txSpinBox->value();

    ui->dataTable->setRowCount(txCount);
    ui->dataTable->setColumnCount(rxCount);

    // 更新行标题
    QStringList rowHeaders;
    for (int i = 0; i < txCount; ++i) {
        rowHeaders << QString("TX%1").arg(i);
    }
    ui->dataTable->setVerticalHeaderLabels(rowHeaders);

    // 更新列标题
    QStringList colHeaders;
    for (int i = 0; i < rxCount; ++i) {
        colHeaders << QString("RX%1").arg(i);
    }
    ui->dataTable->setHorizontalHeaderLabels(colHeaders);
}

void FunctionPage::loadConfig()
{
    QFile file("config.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return; // 如果文件不存在，使用UI中的默认值
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        return;
    }

    QJsonObject root = doc.object();
    QJsonObject params = root["parameters"].toObject();

    // 加载参数值
    if (params.contains("rx_count")) {
        ui->rxSpinBox->setValue(params["rx_count"].toInt());
    }
    if (params.contains("tx_count")) {
        ui->txSpinBox->setValue(params["tx_count"].toInt());
    }
    if (params.contains("raw_threshold")) {
        ui->rawThresholdSpinBox->setValue(params["raw_threshold"].toInt());
    }
    if (params.contains("signal_threshold")) {
        ui->signalThresholdSpinBox->setValue(params["signal_threshold"].toInt());
    }
}

void FunctionPage::saveConfig()
{
    QJsonObject params;
    params["rx_count"] = ui->rxSpinBox->value();
    params["tx_count"] = ui->txSpinBox->value();
    params["raw_threshold"] = ui->rawThresholdSpinBox->value();
    params["signal_threshold"] = ui->signalThresholdSpinBox->value();

    QJsonObject root;
    root["parameters"] = params;

    QJsonDocument doc(root);

    QFile file("config.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}
