#ifndef FUNCTIONPAGE_H
#define FUNCTIONPAGE_H

#include <QWidget>
#include <QTimer>
#include <QVector>

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

    enum DataMode {
        RawData,
        SignalData,
        BaselineData
    };

signals:
    void backToMainPage();

private slots:
    void onBackButtonClicked();
    void onRxCountChanged(int value);
    void onTxCountChanged(int value);
    void onRawThresholdChanged(int value);
    void onSignalThresholdChanged(int value);
    void validateHexInput();
    void validateIntInput();
    void validatePlaySpeed();
    void onBaselineFileBrowse();
    void onTouchFileBrowse();
    void onPlayPauseClicked();
    void onReplayClicked();
    void onPrevFrameClicked();
    void onNextFrameClicked();
    void onRawDataButtonClicked();
    void onSignalDataButtonClicked();
    void onBaselineDataButtonClicked();
    void onBaselineReadButtonClicked();
    void onTouchReadButtonClicked();
    void onPlayTimerTimeout();

private:
    void initializeTable();
    void updateTableSize();
    void loadConfig();
    void saveConfig();
    void updateDataModeButtons();
    bool readBaselineData();
    bool readTouchData();
    QVector<qint16> parseCSVLine(const QString &line, int startPos, int count, bool isBigEndian);
    bool matchFilterPattern(const QStringList &data, int startPos, const QVector<QString> &pattern);
    void displayDataInTable(const QVector<qint16> &data, bool asHex);
    void applySignalDataColors(const QVector<qint16> &data, int rxCount, int txCount);
    void displayCurrentFrame();
    void updateFrameButtons();
    void updateProgressBar();
    void updatePlaySpeed();
    void startPlayback();
    void stopPlayback();

    Ui::FunctionPage *ui;
    bool isPlaying;
    DataMode currentDataMode;

    // 数据存储
    QVector<qint16> baselineData;           // 基线数据
    QVector<QVector<qint16>> touchFrames;   // 触摸数据帧
    int currentFrame;                        // 当前帧索引
    QTimer *playTimer;                       // 播放定时器
};

#endif // FUNCTIONPAGE_H
