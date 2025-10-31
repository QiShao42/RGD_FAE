#include "functionpage.h"
#include "./ui_functionpage.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHeaderView>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QTableWidgetItem>
#include <QQueue>
#include <QColor>
#include <QElapsedTimer>
#include <QDebug>

// 性能调试开关：设置为 false 可以禁用所有调试输出，提升性能
#define ENABLE_PERFORMANCE_DEBUG true

#if ENABLE_PERFORMANCE_DEBUG
    #define PERF_DEBUG(msg) qDebug() << msg
#else
    #define PERF_DEBUG(msg) do {} while(0)
#endif

FunctionPage::FunctionPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FunctionPage)
    , isPlaying(false)
    , currentDataMode(RawData)
    , currentFrame(0)
    , playTimer(new QTimer(this))
{
    ui->setupUi(this);

    // 配置播放定时器（初始间隔50ms，实际值会在loadConfig中设置）
    playTimer->setInterval(50);
    connect(playTimer, &QTimer::timeout, this, &FunctionPage::onPlayTimerTimeout);

    // 设置上下区域的 8:2 比例
    ui->contentVerticalLayout->setStretch(0, 8);  // topArea
    ui->contentVerticalLayout->setStretch(1, 0);  // dividerLine
    ui->contentVerticalLayout->setStretch(2, 2);  // bottomArea

    // 设置 bottomArea 的三个区域比例为 1:1:1
    ui->bottomAreaLayout->setStretch(0, 1);  // bottom_1
    ui->bottomAreaLayout->setStretch(1, 0);  // divider1
    ui->bottomAreaLayout->setStretch(2, 1);  // bottom_2
    ui->bottomAreaLayout->setStretch(3, 0);  // divider2
    ui->bottomAreaLayout->setStretch(4, 1);  // bottom_3

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
    connect(ui->reverseRxCheckBox, &QCheckBox::stateChanged, this, [this]() {
        saveConfig();
        displayCurrentFrame();  // 重新显示数据（表头不变）
    });

    // 连接 hex 输入框的验证
    connect(ui->hexInput1, &QLineEdit::editingFinished, this, &FunctionPage::validateHexInput);
    connect(ui->hexInput2, &QLineEdit::editingFinished, this, &FunctionPage::validateHexInput);
    connect(ui->hexInput3, &QLineEdit::editingFinished, this, &FunctionPage::validateHexInput);
    connect(ui->hexInput4, &QLineEdit::editingFinished, this, &FunctionPage::validateHexInput);
    connect(ui->hexInput5, &QLineEdit::editingFinished, this, &FunctionPage::validateHexInput);

    // 连接整数输入框的验证
    connect(ui->rawDataPosLineEdit, &QLineEdit::editingFinished, this, &FunctionPage::validateIntInput);
    connect(ui->filterStartLineEdit, &QLineEdit::editingFinished, this, &FunctionPage::validateIntInput);
    connect(ui->maxRowsLineEdit, &QLineEdit::editingFinished, this, &FunctionPage::validateIntInput);
    connect(ui->playSpeedLineEdit, &QLineEdit::editingFinished, this, &FunctionPage::validatePlaySpeed);

    // 连接文件浏览按钮
    connect(ui->baselineFileBrowseButton, &QPushButton::clicked, this, &FunctionPage::onBaselineFileBrowse);
    connect(ui->touchFileBrowseButton, &QPushButton::clicked, this, &FunctionPage::onTouchFileBrowse);

    // 连接读取按钮
    connect(ui->baselineReadButton, &QPushButton::clicked, this, &FunctionPage::onBaselineReadButtonClicked);
    connect(ui->touchReadButton, &QPushButton::clicked, this, &FunctionPage::onTouchReadButtonClicked);

    // 连接播放控制按钮
    connect(ui->playPauseButton, &QPushButton::clicked, this, &FunctionPage::onPlayPauseClicked);
    connect(ui->replayButton, &QPushButton::clicked, this, &FunctionPage::onReplayClicked);
    connect(ui->prevFrameButton, &QPushButton::clicked, this, &FunctionPage::onPrevFrameClicked);
    connect(ui->nextFrameButton, &QPushButton::clicked, this, &FunctionPage::onNextFrameClicked);

    // 连接 bottom_1 配置项的信号
    connect(ui->byteOrderComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { saveConfig(); });
    connect(ui->autoFilterSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { saveConfig(); });
    connect(ui->filterModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { saveConfig(); });

    // 连接数据模式切换按钮
    connect(ui->rawDataButton, &QPushButton::clicked, this, &FunctionPage::onRawDataButtonClicked);
    connect(ui->signalDataButton, &QPushButton::clicked, this, &FunctionPage::onSignalDataButtonClicked);
    connect(ui->baselineDataButton, &QPushButton::clicked, this, &FunctionPage::onBaselineDataButtonClicked);

    // 默认选中"原始数据"按钮
    onRawDataButtonClicked();

    // 初始化播放控制按钮状态
    updateFrameButtons();
    updateProgressBar();
}

FunctionPage::~FunctionPage()
{
    stopPlayback();
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

    // 设置列标题 (RX0, RX1, ...) - 始终保持正序
    QStringList colHeaders;
    for (int i = 0; i < rxCount; ++i) {
        colHeaders << QString("RX%1").arg(i);
    }
    ui->dataTable->setHorizontalHeaderLabels(colHeaders);

    // 设置单元格最小尺寸（增加最小列宽以容纳更长的数字）
    ui->dataTable->verticalHeader()->setMinimumSectionSize(18);
    ui->dataTable->horizontalHeader()->setMinimumSectionSize(35); // 增加到60以容纳5-6位数字

    // 设置表头调整模式为自动拉伸
    ui->dataTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->dataTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 设置文字换行模式为不换行，让内容完整显示
    ui->dataTable->setWordWrap(false);

    // 预创建所有单元格，避免后续重复创建
    for (int tx = 0; tx < txCount; ++tx) {
        for (int rx = 0; rx < rxCount; ++rx) {
            QTableWidgetItem *item = new QTableWidgetItem("");
            item->setTextAlignment(Qt::AlignCenter);
            ui->dataTable->setItem(tx, rx, item);
        }
    }
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

    // 更新列标题 - 始终保持正序
    QStringList colHeaders;
    for (int i = 0; i < rxCount; ++i) {
        colHeaders << QString("RX%1").arg(i);
    }
    ui->dataTable->setHorizontalHeaderLabels(colHeaders);

    // 确保所有单元格都已创建
    for (int tx = 0; tx < txCount; ++tx) {
        for (int rx = 0; rx < rxCount; ++rx) {
            if (!ui->dataTable->item(tx, rx)) {
                QTableWidgetItem *item = new QTableWidgetItem("");
                item->setTextAlignment(Qt::AlignCenter);
                ui->dataTable->setItem(tx, rx, item);
            }
        }
    }
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
    if (params.contains("reverse_rx")) {
        ui->reverseRxCheckBox->setChecked(params["reverse_rx"].toBool());
    }

    // 加载 bottom_1 配置
    if (params.contains("raw_data_pos")) {
        ui->rawDataPosLineEdit->setText(QString::number(params["raw_data_pos"].toInt()));
    }
    if (params.contains("byte_order")) {
        ui->byteOrderComboBox->setCurrentIndex(params["byte_order"].toInt());
    }
    if (params.contains("auto_filter_bits")) {
        ui->autoFilterSpinBox->setValue(params["auto_filter_bits"].toInt());
    }
    if (params.contains("filter_start_pos")) {
        ui->filterStartLineEdit->setText(QString::number(params["filter_start_pos"].toInt()));
    }
    if (params.contains("filter_mode")) {
        ui->filterModeComboBox->setCurrentIndex(params["filter_mode"].toInt());
    }
    if (params.contains("hex_input1")) {
        ui->hexInput1->setText(params["hex_input1"].toString());
    }
    if (params.contains("hex_input2")) {
        ui->hexInput2->setText(params["hex_input2"].toString());
    }
    if (params.contains("hex_input3")) {
        ui->hexInput3->setText(params["hex_input3"].toString());
    }
    if (params.contains("hex_input4")) {
        ui->hexInput4->setText(params["hex_input4"].toString());
    }
    if (params.contains("hex_input5")) {
        ui->hexInput5->setText(params["hex_input5"].toString());
    }

    // 加载 bottom_3 文件路径
    if (params.contains("baseline_file_path")) {
        ui->baselineFileLineEdit->setText(params["baseline_file_path"].toString());
    }
    if (params.contains("touch_file_path")) {
        ui->touchFileLineEdit->setText(params["touch_file_path"].toString());
    }

    // 加载最大读取行数
    if (params.contains("max_rows")) {
        int maxRows = params["max_rows"].toInt();
        if (maxRows < 1) maxRows = 1; // 确保至少为1
        ui->maxRowsLineEdit->setText(QString::number(maxRows));
    }

    // 加载播放速度
    if (params.contains("play_speed")) {
        int playSpeed = params["play_speed"].toInt();
        if (playSpeed < 10) playSpeed = 200; // 默认200ms
        ui->playSpeedLineEdit->setText(QString::number(playSpeed));
        playTimer->setInterval(playSpeed);
    }
}

void FunctionPage::saveConfig()
{
    QJsonObject params;
    params["rx_count"] = ui->rxSpinBox->value();
    params["tx_count"] = ui->txSpinBox->value();
    params["raw_threshold"] = ui->rawThresholdSpinBox->value();
    params["signal_threshold"] = ui->signalThresholdSpinBox->value();
    params["reverse_rx"] = ui->reverseRxCheckBox->isChecked();

    // 保存 bottom_1 配置
    params["raw_data_pos"] = ui->rawDataPosLineEdit->text().toInt();
    params["byte_order"] = ui->byteOrderComboBox->currentIndex();
    params["auto_filter_bits"] = ui->autoFilterSpinBox->value();
    params["filter_start_pos"] = ui->filterStartLineEdit->text().toInt();
    params["filter_mode"] = ui->filterModeComboBox->currentIndex();
    params["hex_input1"] = ui->hexInput1->text();
    params["hex_input2"] = ui->hexInput2->text();
    params["hex_input3"] = ui->hexInput3->text();
    params["hex_input4"] = ui->hexInput4->text();
    params["hex_input5"] = ui->hexInput5->text();

    // 保存 bottom_3 文件路径
    params["baseline_file_path"] = ui->baselineFileLineEdit->text();
    params["touch_file_path"] = ui->touchFileLineEdit->text();

    // 保存最大读取行数
    params["max_rows"] = ui->maxRowsLineEdit->text().toInt();

    // 保存播放速度
    params["play_speed"] = ui->playSpeedLineEdit->text().toInt();

    QJsonObject root;
    root["parameters"] = params;

    QJsonDocument doc(root);

    QFile file("config.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

void FunctionPage::validateHexInput()
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit*>(sender());
    if (!lineEdit) {
        return;
    }

    QString text = lineEdit->text().trimmed().toUpper();

    // 如果输入为空或长度不是2，设置为 "00"
    if (text.isEmpty() || text.length() != 2) {
        lineEdit->setText("00");
        saveConfig();
        return;
    }

    // 验证是否为有效的十六进制数
    bool ok;
    int value = text.toInt(&ok, 16);

    if (!ok || value < 0x00 || value > 0xFF) {
        // 如果无效，设置为 "00"
        lineEdit->setText("00");
    } else {
        // 格式化为两位大写十六进制
        lineEdit->setText(QString("%1").arg(value, 2, 16, QChar('0')).toUpper());
    }

    saveConfig();
}

void FunctionPage::validateIntInput()
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit*>(sender());
    if (!lineEdit) {
        return;
    }

    QString text = lineEdit->text().trimmed();

    // 判断是否为最大读取行数输入框（最小值为1）
    bool isMaxRows = (lineEdit == ui->maxRowsLineEdit);
    int minValue = isMaxRows ? 1 : 0;

    // 如果输入为空，设置为最小值
    if (text.isEmpty()) {
        lineEdit->setText(QString::number(minValue));
        saveConfig();
        return;
    }

    // 验证是否为有效的整数
    bool ok;
    int value = text.toInt(&ok);

    if (!ok || value < minValue || value > 32767) {
        // 如果无效或超出范围，限制在 minValue-32767
        if (!ok || value < minValue) {
            lineEdit->setText(QString::number(minValue));
        } else {
            lineEdit->setText("32767");
        }
    } else {
        // 格式化为整数字符串
        lineEdit->setText(QString::number(value));
    }

    saveConfig();
}

void FunctionPage::validatePlaySpeed()
{
    QLineEdit *lineEdit = ui->playSpeedLineEdit;
    QString text = lineEdit->text().trimmed();

    // 最小值为10ms，避免播放过快
    int minValue = 10;

    // 如果输入为空，设置为默认值200
    if (text.isEmpty()) {
        lineEdit->setText("200");
        updatePlaySpeed();
        return;
    }

    // 验证是否为有效的整数
    bool ok;
    int value = text.toInt(&ok);

    if (!ok || value < minValue || value > 10000) {
        // 如果无效或超出范围（10-10000ms）
        if (!ok || value < minValue) {
            lineEdit->setText(QString::number(minValue));
        } else {
            lineEdit->setText("10000");
        }
    } else {
        // 格式化为整数字符串
        lineEdit->setText(QString::number(value));
    }

    updatePlaySpeed();
}

void FunctionPage::updatePlaySpeed()
{
    int speed = ui->playSpeedLineEdit->text().toInt();
    if (speed < 10) speed = 200; // 保护机制

    // 如果定时器正在运行，需要重启才能使新间隔生效
    bool wasRunning = playTimer->isActive();
    if (wasRunning) {
        playTimer->stop();
    }

    playTimer->setInterval(speed);

    if (wasRunning) {
        playTimer->start();
    }

    saveConfig();
}

void FunctionPage::onBaselineFileBrowse()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("选择基线数据文件"),
        "",
        tr("CSV文件 (*.csv);;所有文件 (*.*)")
    );

    if (!fileName.isEmpty()) {
        ui->baselineFileLineEdit->setText(fileName);
        saveConfig();
    }
}

void FunctionPage::onTouchFileBrowse()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("选择触摸数据文件"),
        "",
        tr("CSV文件 (*.csv);;所有文件 (*.*)")
    );

    if (!fileName.isEmpty()) {
        ui->touchFileLineEdit->setText(fileName);
        saveConfig();
    }
}

void FunctionPage::updateDataModeButtons()
{
    // 定义选中和未选中的样式
    QString selectedStyle = "QPushButton {"
                           "    background-color: #66CCFF;"
                           "    color: white;"
                           "    border: 1px solid #66CCFF;"
                           "    border-radius: 3px;"
                           "    font-size: 12px;"
                           "    font-weight: bold;"
                           "}"
                           "QPushButton:hover {"
                           "    background-color: #5ABBFF;"
                           "}";

    QString normalStyle = "QPushButton {"
                         "    background-color: white;"
                         "    color: #003D7A;"
                         "    border: 1px solid #66CCFF;"
                         "    border-radius: 3px;"
                         "    font-size: 12px;"
                         "}"
                         "QPushButton:hover {"
                         "    background-color: #E6F7FF;"
                         "}"
                         "QPushButton:pressed {"
                         "    background-color: #66CCFF;"
                         "    color: white;"
                         "}";

    // 根据当前模式设置按钮样式
    ui->rawDataButton->setStyleSheet(currentDataMode == RawData ? selectedStyle : normalStyle);
    ui->signalDataButton->setStyleSheet(currentDataMode == SignalData ? selectedStyle : normalStyle);
    ui->baselineDataButton->setStyleSheet(currentDataMode == BaselineData ? selectedStyle : normalStyle);
}

void FunctionPage::onRawDataButtonClicked()
{
    currentDataMode = RawData;
    updateDataModeButtons();
    displayCurrentFrame();
}

void FunctionPage::onSignalDataButtonClicked()
{
    currentDataMode = SignalData;
    updateDataModeButtons();
    displayCurrentFrame();
}

void FunctionPage::onBaselineDataButtonClicked()
{
    currentDataMode = BaselineData;
    updateDataModeButtons();
    displayCurrentFrame();
}

void FunctionPage::onBaselineReadButtonClicked()
{
    if (readBaselineData()) {
        QMessageBox::information(this, tr("读取成功"), tr("基线数据读取完成！"));
    } else {
        QMessageBox::warning(this, tr("读取失败"), tr("基线数据读取失败，请检查文件路径和格式！"));
    }
}

bool FunctionPage::matchFilterPattern(const QStringList &data, int startPos, const QVector<QString> &pattern)
{
    // 检查是否有足够的数据进行匹配
    if (startPos + pattern.size() > data.size()) {
        return false;
    }

    // 逐个匹配
    for (int i = 0; i < pattern.size(); ++i) {
        QString dataValue = data[startPos + i].trimmed().toUpper();
        QString patternValue = pattern[i].toUpper();

        // 移除可能的0x前缀
        if (dataValue.startsWith("0X")) {
            dataValue = dataValue.mid(2);
        }
        if (patternValue.startsWith("0X")) {
            patternValue = patternValue.mid(2);
        }

        // 确保是2位十六进制数
        while (dataValue.length() < 2) dataValue = "0" + dataValue;
        while (patternValue.length() < 2) patternValue = "0" + patternValue;

        if (dataValue != patternValue) {
            return false;
        }
    }

    return true;
}

QVector<qint16> FunctionPage::parseCSVLine(const QString &line, int startPos, int count, bool isBigEndian)
{
    QVector<qint16> result;

    // 分割CSV行
    QStringList data = line.split(',');

    // 移除所有数据的空白字符
    for (QString &item : data) {
        item = item.trimmed();
    }

    // 检查是否有足够的数据
    if (startPos >= data.size() || startPos + count > data.size()) {
        return result;
    }

    // 每两个8位数据组成一个16位数据
    for (int i = 0; i < count; i += 2) {
        if (startPos + i + 1 >= data.size()) {
            break;
        }

        // 读取两个8位十六进制数
        bool ok1, ok2;
        QString hex1 = data[startPos + i].trimmed();
        QString hex2 = data[startPos + i + 1].trimmed();

        // 移除可能的0x前缀
        if (hex1.startsWith("0x", Qt::CaseInsensitive)) {
            hex1 = hex1.mid(2);
        }
        if (hex2.startsWith("0x", Qt::CaseInsensitive)) {
            hex2 = hex2.mid(2);
        }

        quint8 byte1 = hex1.toUInt(&ok1, 16);
        quint8 byte2 = hex2.toUInt(&ok2, 16);

        if (!ok1 || !ok2) {
            continue;
        }

        // 根据字节序组合
        qint16 value;
        if (isBigEndian) {
            // 大端：第一个字节是高字节
            value = (byte1 << 8) | byte2;
        } else {
            // 小端：第一个字节是低字节
            value = (byte2 << 8) | byte1;
        }

        result.append(value);
    }

    return result;
}

void FunctionPage::applySignalDataColors(const QVector<qint16> &data, int rxCount, int txCount)
{
    QElapsedTimer timer;
    timer.start();

    int signalThreshold = ui->signalThresholdSpinBox->value();

    // 创建二维数组存储数据（tx行，rx列）
    QElapsedTimer gridTimer;
    gridTimer.start();
    QVector<QVector<qint16>> grid(txCount, QVector<qint16>(rxCount));
    int dataIndex = 0;
    for (int tx = 0; tx < txCount; ++tx) {
        for (int rx = 0; rx < rxCount; ++rx) {
            grid[tx][rx] = data[dataIndex++];
        }
    }

    // 标记访问过的格子和超过阈值的格子
    QVector<QVector<bool>> visited(txCount, QVector<bool>(rxCount, false));
    QVector<QVector<bool>> isPeak(txCount, QVector<bool>(rxCount, false));

    // 8邻域方向（上、下、左、右、左上、右上、左下、右下）
    int dx[] = {-1, 1, 0, 0, -1, -1, 1, 1};
    int dy[] = {0, 0, -1, 1, -1, 1, -1, 1};

    // BFS查找连通域
    QElapsedTimer bfsTimer;
    bfsTimer.start();
    int componentCount = 0;
    for (int tx = 0; tx < txCount; ++tx) {
        for (int rx = 0; rx < rxCount; ++rx) {
            if (!visited[tx][rx] && grid[tx][rx] > signalThreshold) {
                componentCount++;
                // 找到一个新的连通域，使用BFS遍历
                QVector<QPair<int, int>> component; // 存储当前连通域的所有格子
                QQueue<QPair<int, int>> queue;

                queue.enqueue(qMakePair(tx, rx));
                visited[tx][rx] = true;

                while (!queue.isEmpty()) {
                    auto current = queue.dequeue();
                    int ctx = current.first;
                    int crx = current.second;
                    component.append(current);

                    // 检查8邻域
                    for (int d = 0; d < 8; ++d) {
                        int ntx = ctx + dx[d];
                        int nrx = crx + dy[d];

                        // 边界检查
                        if (ntx >= 0 && ntx < txCount && nrx >= 0 && nrx < rxCount) {
                            if (!visited[ntx][nrx] && grid[ntx][nrx] > signalThreshold) {
                                visited[ntx][nrx] = true;
                                queue.enqueue(qMakePair(ntx, nrx));
                            }
                        }
                    }
                }

                // 在当前连通域中检测峰值
                for (const auto &pos : component) {
                    int ctx = pos.first;
                    int crx = pos.second;
                    qint16 centerValue = grid[ctx][crx];
                    bool isLocalPeak = true;

                    // 检查8邻域，看是否为峰值
                    for (int d = 0; d < 8; ++d) {
                        int ntx = ctx + dx[d];
                        int nrx = crx + dy[d];

                        if (ntx >= 0 && ntx < txCount && nrx >= 0 && nrx < rxCount) {
                            if (grid[ntx][nrx] > centerValue) {
                                isLocalPeak = false;
                                break;
                            }
                        }
                    }

                    if (isLocalPeak) {
                        isPeak[ctx][crx] = true;
                    }
                }
            }
        }
    }

    // 应用颜色（使用静态颜色对象避免重复创建）
    QElapsedTimer colorTimer;
    colorTimer.start();
    static QColor peakColor("#32C8B4");      // 青绿色 - 峰值
    static QColor thresholdColor("#FA78E0");  // 粉色 - 超过阈值
    static QColor defaultColor("#FFFFEF");    // 默认背景色

    int colorChanges = 0;
    int thresholdCount = 0;
    int peakCount = 0;
    bool reverseRx = ui->reverseRxCheckBox->isChecked();
    for (int tx = 0; tx < txCount; ++tx) {
        for (int rx = 0; rx < rxCount; ++rx) {
            // 计算实际的列索引（如果反转RX，则应用到反转后的位置）
            int actualRx = reverseRx ? (rxCount - 1 - rx) : rx;
            QTableWidgetItem *item = ui->dataTable->item(tx, actualRx);
            if (item) {
                QColor newColor;
                if (grid[tx][rx] > signalThreshold) {
                    thresholdCount++;
                    if (isPeak[tx][rx]) {
                        newColor = peakColor;
                        peakCount++;
                    } else {
                        newColor = thresholdColor;
                    }
                } else {
                    newColor = defaultColor;
                }

                // 只在颜色不同时才设置，避免不必要的更新
                if (item->background().color() != newColor) {
                    // 使用 setData 设置背景色，优先级更高
                    item->setData(Qt::BackgroundRole, QBrush(newColor));
                    colorChanges++;
                }
            }
        }
    }
}

void FunctionPage::displayDataInTable(const QVector<qint16> &data, bool asHex)
{
    QElapsedTimer totalTimer;
    totalTimer.start();

    int rxCount = ui->rxSpinBox->value();
    int txCount = ui->txSpinBox->value();

    // 检查数据量是否匹配
    if (data.size() != rxCount * txCount) {
        QMessageBox::warning(this, tr("数据量不匹配"),
            tr("读取的数据量(%1)与表格大小(%2x%3=%4)不匹配！")
            .arg(data.size()).arg(rxCount).arg(txCount).arg(rxCount * txCount));
        return;
    }

    // 暂时禁用表格更新，批量修改后一次性刷新
    ui->dataTable->setUpdatesEnabled(false);

    // 按从左到右，从上到下的顺序填充表格
    QElapsedTimer fillTimer;
    fillTimer.start();
    bool reverseRx = ui->reverseRxCheckBox->isChecked();
    int dataIndex = 0;
    for (int tx = 0; tx < txCount; ++tx) {
        for (int rx = 0; rx < rxCount; ++rx) {
            QString text;
            if (asHex) {
                // 16进制显示，将有符号数视为无符号数显示
                quint16 unsignedValue = static_cast<quint16>(data[dataIndex]);
                text = QString("%1").arg(unsignedValue, 4, 16, QChar('0')).toUpper();
            } else {
                // 10进制显示
                text = QString::number(data[dataIndex]);
            }

            // 计算实际填充的列索引（如果反转RX，则从右往左填充）
            int actualRx = reverseRx ? (rxCount - 1 - rx) : rx;

            // 重用已存在的item，直接设置文本（item一定存在，因为已预创建）
            QTableWidgetItem *item = ui->dataTable->item(tx, actualRx);
            item->setText(text);
            dataIndex++;
        }
    }
    PERF_DEBUG("[性能] 填充表格数据耗时:" << fillTimer.elapsed() << "ms");

    // 最后应用背景颜色（在所有表格操作完成之后）
    // 如果是信号数据模式（非16进制），应用背景颜色逻辑
    QElapsedTimer colorTimer;
    colorTimer.start();
    if (!asHex && currentDataMode == SignalData) {
        applySignalDataColors(data, rxCount, txCount);
    } else {
        // 如果不是信号数据模式，清除所有背景色（只在必要时设置）
        static QColor defaultColor("#FFFFEF");
        bool reverseRx = ui->reverseRxCheckBox->isChecked();
        for (int tx = 0; tx < txCount; ++tx) {
            for (int rx = 0; rx < rxCount; ++rx) {
                // 计算实际的列索引
                int actualRx = reverseRx ? (rxCount - 1 - rx) : rx;
                QTableWidgetItem *item = ui->dataTable->item(tx, actualRx);
                if (item) {
                    // 只在颜色不同时才设置，避免不必要的更新
                    if (item->background().color() != defaultColor) {
                        // 使用 setData 设置背景色
                        item->setData(Qt::BackgroundRole, QBrush(defaultColor));
                    }
                }
            }
        }
    }
    // 立即重新启用更新并返回，让 Qt 异步处理重绘
    ui->dataTable->setUpdatesEnabled(true);

    PERF_DEBUG("[性能] displayDataInTable 完整总耗时（含启用更新）:" << totalTimer.elapsed() << "ms");
    PERF_DEBUG("========================================");
}

bool FunctionPage::readBaselineData()
{
    // 获取文件路径
    QString filePath = ui->baselineFileLineEdit->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("文件路径为空"), tr("请先选择基线数据文件！"));
        return false;
    }

    // 打开CSV文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("文件打开失败"), tr("无法打开文件：%1").arg(filePath));
        return false;
    }

    QTextStream in(&file);

    // 获取配置参数
    int autoFilterBits = ui->autoFilterSpinBox->value();
    int filterStartPos = ui->filterStartLineEdit->text().toInt();
    int filterMode = ui->filterModeComboBox->currentIndex(); // 0=本行, 1=下一行
    int rawDataPos = ui->rawDataPosLineEdit->text().toInt();
    bool isBigEndian = (ui->byteOrderComboBox->currentIndex() == 0); // 0=大端, 1=小端
    int rxCount = ui->rxSpinBox->value();
    int txCount = ui->txSpinBox->value();
    int totalBytes = rxCount * txCount * 2; // 需要读取的8位数据总量

    // 构建过滤模式
    QVector<QString> pattern;
    pattern.append(ui->hexInput1->text());
    if (autoFilterBits >= 2) pattern.append(ui->hexInput2->text());
    if (autoFilterBits >= 3) pattern.append(ui->hexInput3->text());
    if (autoFilterBits >= 4) pattern.append(ui->hexInput4->text());
    if (autoFilterBits >= 5) pattern.append(ui->hexInput5->text());

    // 读取并匹配行
    QString matchedLine;
    QString nextLine;
    bool foundMatch = false;

    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList data = line.split(',');

        // 移除空白字符
        for (QString &item : data) {
            item = item.trimmed();
        }

        // 匹配过滤模式
        if (matchFilterPattern(data, filterStartPos, pattern)) {
            matchedLine = line;

            // 如果需要读取下一行，继续读取
            if (filterMode == 1) {
                if (!in.atEnd()) {
                    nextLine = in.readLine();
                }
            }

            foundMatch = true;
            break;
        }
    }

    file.close();

    if (!foundMatch) {
        QMessageBox::warning(this, tr("未找到匹配行"), tr("在文件中未找到符合筛选条件的数据行！"));
        return false;
    }

    // 确定要解析的行
    QString lineToRead = (filterMode == 0) ? matchedLine : nextLine;
    if (lineToRead.isEmpty()) {
        QMessageBox::warning(this, tr("数据行为空"), tr("要读取的数据行为空！"));
        return false;
    }

    // 解析数据
    QVector<qint16> data = parseCSVLine(lineToRead, rawDataPos, totalBytes, isBigEndian);

    if (data.isEmpty() || data.size() != rxCount * txCount) {
        QMessageBox::warning(this, tr("数据解析失败"),
            tr("解析的数据量(%1)与期望值(%2)不符！").arg(data.size()).arg(rxCount * txCount));
        return false;
    }

    // 保存基线数据到成员变量
    baselineData = data;

    // 保存到 baseLine.txt (以16进制格式保存)
    QFile outputFile("baseLine.txt");
    if (outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outputFile);
        for (int i = 0; i < data.size(); ++i) {
            quint16 unsignedValue = static_cast<quint16>(data[i]);
            out << QString("%1").arg(unsignedValue, 4, 16, QChar('0')).toUpper();
            if (i < data.size() - 1) {
                out << "\n";
            }
        }
        outputFile.close();
    }

    // 以16进制显示在表格中
    displayDataInTable(data, true);

    return true;
}

bool FunctionPage::readTouchData()
{
    // 获取文件路径
    QString filePath = ui->touchFileLineEdit->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("文件路径为空"), tr("请先选择触摸数据文件！"));
        return false;
    }

    // 打开CSV文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("文件打开失败"), tr("无法打开文件：%1").arg(filePath));
        return false;
    }

    QTextStream in(&file);

    // 获取配置参数
    int autoFilterBits = ui->autoFilterSpinBox->value();
    int filterStartPos = ui->filterStartLineEdit->text().toInt();
    int filterMode = ui->filterModeComboBox->currentIndex(); // 0=本行, 1=下一行
    int rawDataPos = ui->rawDataPosLineEdit->text().toInt();
    bool isBigEndian = (ui->byteOrderComboBox->currentIndex() == 0); // 0=大端, 1=小端
    int rxCount = ui->rxSpinBox->value();
    int txCount = ui->txSpinBox->value();
    int totalBytes = rxCount * txCount * 2; // 需要读取的8位数据总量
    int maxRows = ui->maxRowsLineEdit->text().toInt();

    // 构建过滤模式
    QVector<QString> pattern;
    pattern.append(ui->hexInput1->text());
    if (autoFilterBits >= 2) pattern.append(ui->hexInput2->text());
    if (autoFilterBits >= 3) pattern.append(ui->hexInput3->text());
    if (autoFilterBits >= 4) pattern.append(ui->hexInput4->text());
    if (autoFilterBits >= 5) pattern.append(ui->hexInput5->text());

    // 清空之前的触摸数据
    touchFrames.clear();

    // 读取并匹配行
    int framesRead = 0;
    QString currentLine;
    QString nextLine;

    while (!in.atEnd() && framesRead < maxRows) {
        currentLine = in.readLine();
        QStringList data = currentLine.split(',');

        // 移除空白字符
        for (QString &item : data) {
            item = item.trimmed();
        }

        // 匹配过滤模式
        if (matchFilterPattern(data, filterStartPos, pattern)) {
            QString lineToRead;

            // 确定要解析的行
            if (filterMode == 0) {
                // 读取匹配行本行
                lineToRead = currentLine;
            } else {
                // 读取匹配行的下一行
                if (!in.atEnd()) {
                    nextLine = in.readLine();
                    lineToRead = nextLine;
                } else {
                    break; // 没有下一行了
                }
            }

            // 解析数据
            QVector<qint16> frameData = parseCSVLine(lineToRead, rawDataPos, totalBytes, isBigEndian);

            if (frameData.size() == rxCount * txCount) {
                touchFrames.append(frameData);
                framesRead++;
            }
        }
    }

    file.close();

    if (touchFrames.isEmpty()) {
        QMessageBox::warning(this, tr("未找到数据"), tr("在文件中未找到符合筛选条件的数据！"));
        return false;
    }

    // 保存到 touchData.txt (以16进制格式保存所有帧)
    QFile outputFile("touchData.txt");
    if (outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&outputFile);
        for (int frame = 0; frame < touchFrames.size(); ++frame) {
            out << "Frame " << frame << ":\n";
            const QVector<qint16> &frameData = touchFrames[frame];
            for (int i = 0; i < frameData.size(); ++i) {
                quint16 unsignedValue = static_cast<quint16>(frameData[i]);
                out << QString("%1").arg(unsignedValue, 4, 16, QChar('0')).toUpper();
                if (i < frameData.size() - 1) {
                    out << "\n";
                } else if (frame < touchFrames.size() - 1) {
                    out << "\n\n";
                }
            }
        }
        outputFile.close();
    }

    // 重置播放状态
    currentFrame = 0;
    stopPlayback();

    // 显示第一帧
    displayCurrentFrame();

    // 更新按钮和进度条状态
    updateFrameButtons();
    updateProgressBar();

    QMessageBox::information(this, tr("读取成功"),
        tr("成功读取 %1 帧触摸数据！").arg(touchFrames.size()));

    return true;
}

void FunctionPage::displayCurrentFrame()
{
    QElapsedTimer timer;
    timer.start();

    if (touchFrames.isEmpty() || currentFrame < 0 || currentFrame >= touchFrames.size()) {
        return;
    }

    const QVector<qint16> &frameData = touchFrames[currentFrame];

    PERF_DEBUG("======== 显示第" << (currentFrame + 1) << "帧 ========");

    if (currentDataMode == RawData) {
        // 原始数据：以16进制显示
        PERF_DEBUG("[性能] 当前模式: 原始数据 (16进制)");
        displayDataInTable(frameData, true);
    } else if (currentDataMode == SignalData) {
        // 信号数据：基线数据 - 原始数据，以10进制显示
        PERF_DEBUG("[性能] 当前模式: 信号数据 (10进制)");
        if (baselineData.size() == frameData.size()) {
            QElapsedTimer calcTimer;
            calcTimer.start();
            QVector<qint16> signalData;
            for (int i = 0; i < frameData.size(); ++i) {
                signalData.append(baselineData[i] - frameData[i]);
            }
            PERF_DEBUG("[性能] 计算信号数据耗时:" << calcTimer.elapsed() << "ms");
            displayDataInTable(signalData, false);
        } else {
            QMessageBox::warning(this, tr("数据不匹配"), tr("基线数据尚未读取或大小不匹配！"));
        }
    } else if (currentDataMode == BaselineData) {
        // 基线数据：以16进制显示
        PERF_DEBUG("[性能] 当前模式: 基线数据 (16进制)");
        if (!baselineData.isEmpty()) {
            displayDataInTable(baselineData, true);
        }
    }

    PERF_DEBUG("[性能] displayCurrentFrame 总耗时:" << timer.elapsed() << "ms");
    PERF_DEBUG("");
}

void FunctionPage::updateFrameButtons()
{
    bool hasFrames = !touchFrames.isEmpty();

    // 只在状态改变时才更新，减少不必要的 UI 刷新
    bool prevEnabled = hasFrames && currentFrame > 0;
    bool nextEnabled = hasFrames && currentFrame < touchFrames.size() - 1;

    if (ui->prevFrameButton->isEnabled() != prevEnabled) {
        ui->prevFrameButton->setEnabled(prevEnabled);
    }
    if (ui->nextFrameButton->isEnabled() != nextEnabled) {
        ui->nextFrameButton->setEnabled(nextEnabled);
    }
    if (ui->playPauseButton->isEnabled() != hasFrames) {
        ui->playPauseButton->setEnabled(hasFrames);
    }
    if (ui->replayButton->isEnabled() != hasFrames) {
        ui->replayButton->setEnabled(hasFrames);
    }
}

void FunctionPage::updateProgressBar()
{
    QElapsedTimer timer;
    timer.start();

    static int lastCurrentValue = -1;
    static int lastMaxValue = -1;

    if (touchFrames.isEmpty()) {
        // 没有数据时显示 0 / 0
        if (lastCurrentValue != 0 || lastMaxValue != 0) {
            ui->frameInfoLabel->setText("0 / 0");
            ui->progressBarFill->setMaximumWidth(0);
            lastCurrentValue = 0;
            lastMaxValue = 0;
        }
    } else {
        // 有数据时显示当前帧 / 总帧数
        int maxValue = touchFrames.size();
        int currentValue = currentFrame + 1; // +1 因为显示从1开始

        // 只在值改变时才更新
        if (currentValue != lastCurrentValue || maxValue != lastMaxValue) {
            // 更新文本
            ui->frameInfoLabel->setText(QString("%1 / %2").arg(currentValue).arg(maxValue));
            // 更新自定义进度条宽度（通过百分比）
            // 使用 stretch factor 来控制填充条的比例
            int containerWidth = ui->progressBarContainer->width() - 8; // 减去边距和边框
            if (containerWidth > 0) {
                int fillWidth = (containerWidth * currentValue) / maxValue;
                ui->progressBarFill->setMinimumWidth(fillWidth);
                ui->progressBarFill->setMaximumWidth(fillWidth);
            }
            lastCurrentValue = currentValue;
            lastMaxValue = maxValue;
        }
    }
    PERF_DEBUG("[性能] updateProgressBar 总耗时:" << timer.elapsed() << "ms");
}

void FunctionPage::startPlayback()
{
    if (!touchFrames.isEmpty()) {
        isPlaying = true;
        ui->playPauseButton->setText("暂停");
        playTimer->start();
    }
}

void FunctionPage::stopPlayback()
{
    isPlaying = false;
    ui->playPauseButton->setText("播放");
    playTimer->stop();
}

void FunctionPage::onTouchReadButtonClicked()
{
    if (readTouchData()) {
        // 读取成功后的处理已在readTouchData中完成
    }
}

void FunctionPage::onPlayPauseClicked()
{
    if (touchFrames.isEmpty()) {
        return;
    }

    if (isPlaying) {
        stopPlayback();
    } else {
        startPlayback();
    }
}

void FunctionPage::onReplayClicked()
{
    if (touchFrames.isEmpty()) {
        return;
    }

    // 停止当前播放
    stopPlayback();

    // 从第一帧开始播放
    currentFrame = 0;
    displayCurrentFrame();
    updateFrameButtons();
    updateProgressBar();
    startPlayback();
}

void FunctionPage::onPrevFrameClicked()
{
    QElapsedTimer timer;
    timer.start();

    if (touchFrames.isEmpty() || currentFrame <= 0) {
        return;
    }

    currentFrame--;

    QElapsedTimer btnTimer;
    btnTimer.start();
    updateFrameButtons();

    QElapsedTimer progTimer;
    progTimer.start();
    updateProgressBar();

    displayCurrentFrame();

    PERF_DEBUG("[按钮] 上一帧按钮点击处理总耗时:" << timer.elapsed() << "ms\n");
}

void FunctionPage::onNextFrameClicked()
{
    QElapsedTimer timer;
    timer.start();

    if (touchFrames.isEmpty() || currentFrame >= touchFrames.size() - 1) {
        return;
    }

    currentFrame++;

    QElapsedTimer btnTimer;
    btnTimer.start();
    updateFrameButtons();

    QElapsedTimer progTimer;
    progTimer.start();
    updateProgressBar();

    displayCurrentFrame();

    PERF_DEBUG("[按钮] 下一帧按钮点击处理总耗时:" << timer.elapsed() << "ms\n");
}

void FunctionPage::onPlayTimerTimeout()
{
    QElapsedTimer timer;
    timer.start();

    if (touchFrames.isEmpty()) {
        stopPlayback();
        return;
    }

    // 移动到下一帧
    currentFrame++;

    // 如果到达最后一帧，停止播放
    if (currentFrame >= touchFrames.size()) {
        currentFrame = touchFrames.size() - 1;
        stopPlayback();
    }

    displayCurrentFrame();
    updateFrameButtons();
    PERF_DEBUG("[性能] updateFrameButtons 耗时:" << timer.elapsed() << "ms");
    updateProgressBar();

    PERF_DEBUG("[播放] 定时器触发处理总耗时:" << timer.elapsed() << "ms (播放速度设置:" << playTimer->interval() << "ms)\n");
}
