#ifndef MODBUSWORKER_H
#define MODBUSWORKER_H
#include<QObject>
#include<QSerialPort>
#include<QTimer>

class ModbusWorker:public QObject
{
    Q_OBJECT
public:
      explicit  ModbusWorker(QObject*parent=0);
    ~ModbusWorker();
    //槽函数
public slots:
    //子线程的槽函数
    void doOpenSerial(const QString&portname,int baud);
    void doCloseSerial();
    void doReadRequest(int slaveaddr,int startaddr,int numregs);
    void doWriteRequest(int salveaddr,int regaddr,int value);
       void On_timeout();
    void starPolling(int intervalMS);//启动轮询
    void stopPolling();
  void onPollTimer();//轮询超时槽,发送读请求
    //传递给主线程的信号
signals:
    void serialOpened(bool success,const QString&msg);
    void serialClosed();
    void readCompleted(int value);
    void writeCompleted(bool success,const QString&msg);
    void error(const QString&error);

private:
    //成员属性
    QSerialPort*serial;
    QTimer*polltimer;//轮询指针
    QByteArray recvBuffer;//缓冲区
    QByteArray lastRequest;//发送请求
    QTimer*timeoutTimer;//超时定时器
    bool waitingforresponse;//判断是否在等待响应
    int retryCount;//重试次数
    const int maxRetries=3;//最大重试次数
    int pollInterval;//轮询间隔
    //功能函数
    QByteArray bulidModbusRequest03(int slaveAddr,int StartAddr,int numRegs);
    uint16_t crc16_modbus(const uint8_t* data, uint16_t length);//CRC校验
    void setupm_ui();//创建一个可维护的UI界面容器
    void updateSerialPorts();//扫描串口函数
    QByteArray bulidModbusRequest06(int slaveaddr,int regaddr,int value);
     void processFrame(const QByteArray&frame);
     void onReadyRead();


};

#endif // MODBUSWORKER_H
