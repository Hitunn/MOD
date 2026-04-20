    #ifndef MAINWINDOW_H
    #define MAINWINDOW_H

    #include <QMainWindow>
    #include<QtSerialPort/QSerialPort>
    #include<QTimer>
    QT_BEGIN_NAMESPACE
    class QComboBox;
    class QLineEdit;
    class QPushButton;
    class QTableWidget;
    class QLabel;
    class QVBoxLayout;
    QT_END_NAMESPACE

    namespace Ui {
    class MainWindow;
    }

    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = 0);
        ~MainWindow();
    private slots:
        void onOpenserial();
        void onCloseserial();
        void onStarTimer();
        void onStopTimer();
        void onSendData();
        void onReadyRead();
          void on_readModbusRequest03();
          void on_writeButton_clicked();
          void On_timeout();

    private:
        Ui::MainWindow *ui;
        QSerialPort*serial;
        QTimer*timer;
        int recordcount;
        void setupm_ui();//创建一个可维护的UI界面容器
        void updateSerialPorts();//扫描串口函数
        QByteArray recvBuffer;//缓冲区
        QByteArray lastRequest;
        QTimer*timeoutTimer;//超时定时器
        bool waitingforresponse;//判断是否在等待响应
        int retryCount;//重试次数
        const int maxRetries=3;//最大重试次数
        //功能函数
        QByteArray bulidModbusRequest03(int slaveAddr,int StartAddr,int numRegs);
        uint16_t crc16_modbus(const uint8_t* data, uint16_t length);//CRC校验

        QByteArray bulidModbusRequest06(int slaveaddr,int regaddr,int value);
         void processFrame(const QByteArray&frame);


        //控件指针
        QComboBox*portComboBox;
        QComboBox*baudComboBox;
        QPushButton*closeButton;
        QPushButton*openButton;
        QPushButton*startTimerButton;
        QPushButton*stopTimerButton;
        QLineEdit*sendLineEdit;
        QTableWidget*tableWidget;
        QVBoxLayout*mainLayout;
        QPushButton*readModbusButton;
        QLabel*ModbusValueLabel;
        QLineEdit*writeAddr;
        QLineEdit*writeValue;
        QPushButton*writeButton;


};

#endif // MAINWINDOW_H
