    #ifndef MAINWINDOW_H
    #define MAINWINDOW_H

    #include <QMainWindow>
    #include<QtSerialPort/QSerialPort>
    #include<QTimer>
    #include<QThread>
#include<QSpinBox>
#include<modbusworker.h>
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
        int recordcount;//计数器
    private slots:
        //主线程的点击按钮槽函数
        void onOpenButtonClicked();//打开串口按钮的槽函数,用于发出信号给子线程
        void onCloseButtonCicked();
        void onReadButtonClicked();
        void onwriteButtonClicked();
        void onStartPollingButtonClicked();
        void onStopPollingButtonCicked();
        //接受工作线程发送过来的信号的槽函数,用于更新UI比如打开成功的提醒
        void onSeiralOpened(bool success,const QString&msg);
        void onSeiralClosed();
        void onReadCompleted(int value);//接受信号传输过来的寄存器值,需要形参
        void onWriteComplted(bool success,const QString&msg);
        void onError(const QString&error);
    signals:
        //自定义信号,方便跨线程,主要是关联UI上的界面事件和工作线程的连接
        void sigOpenSerial(const QString&portname, int Baud);
        void sigCloseSerial();
        void sigReadRequest(int slaveaddr,int startaddr,int numregs);
        void sigWriteRequest(int salveaddr,int regaddr,int value);
        void sigStarPolling(int interval);
        void sigStopPolling();
        void onSendData();
    private:
        Ui::MainWindow *ui;
        QThread*workerThread;
        ModbusWorker*worker;
        void setupm_ui();//创建一个可维护的UI界面容器
        void updateSerialPorts();//扫描串口函数
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
        QSpinBox*pollIntervalSpinbox;
};

#endif // MAINWINDOW_H
