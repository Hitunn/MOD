#ifndef MODBUSWORKER_H
#define MODBUSWORKER_H
#include<QObject>
#include<QSerialPort>
#include<QTimer>
#include<QQueue>
#include<QList>
#include<QMap>

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
    void doReadRequest(int slaveaddr,int startaddr,int numregs,bool isHeartbeat,bool allowRetry);
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
    void deviceOnline();
    void deviceOffline();
private slots:
     void onSlaveHeartbeatTimeout(int slaveId);   // 心跳超时槽函数，接收从站ID

private:
    //成员属性
    QSerialPort*serial;//串口指针
    QTimer*polltimer;//轮询指针
    //分包粘包
    QByteArray recvBuffer;//缓冲区
    QByteArray lastRequest;//发送请求
    //超时重试
    QTimer*timeoutTimer;//超时定时器
    int retryCount;//重试次数
    const int maxRetries=3;//最大重试次数
    int pollInterval;//轮询间隔
    //心跳定时器
    QMap<int, QTimer*> m_slaveHeartbeatTimers;   // 每个从站独立心跳定时器
    void initSlaveHeartbeatTimers();             // 初始化所有从站的心跳定时器
    int heartbeatFailCout;//心跳失败次数
    const int maxHeartbeatFails=3;//最大失败次数
    bool isDeviceOnline;//设备在线状态
    int onlineHeartbeatInterval;//在线时轮询间隔
    int offlineHeartbeatInterval;//离线轮询间隔
    bool lastRequestHeartbeat;//心跳标记
    bool currentAllowRetry;//区分轮询,心跳,单次点击的标记
    //请求优先级枚举
    enum ReqPriority {
        PRIO_HEARTBEAT = 0,  // 最高，可抢占其他
        PRIO_MANUAL,         // 用户手动操作，不可被抢占
        PRIO_POLL            // 最低，可被心跳抢占
    };
    ReqPriority currentReqPriority;//当前优先级
    // Modbus 请求结构体
    struct ModbusRequest {
        int slaveId;
        quint8 funcCode;
        quint16 startAddr;
        quint16 valueOrCount;   // 读数量或写值
        bool isHeartbeat;
        bool allowRetry;
        int retryCount;
        ReqPriority priority;
//无参构造
        ModbusRequest() : slaveId(0), funcCode(0), startAddr(0), valueOrCount(0),
                            isHeartbeat(false), allowRetry(false), retryCount(0), priority(PRIO_POLL) {}
        //有参构造
        ModbusRequest(int id, quint8 func, quint16 addr, quint16 valOrCnt,
                      bool heartbeat, bool retry, ReqPriority prio)
            : slaveId(id), funcCode(func), startAddr(addr), valueOrCount(valOrCnt),
              isHeartbeat(heartbeat), allowRetry(retry), retryCount(0), priority(prio) {}
    };
    QQueue<ModbusRequest> m_requestQueue;   // 请求队列
    bool m_busy;                            // 是否正在等待响应,替代 waitingforresponse
    ModbusRequest m_currentReq;             // 当前正在处理的请求
    //多从站轮询数组
    QList<int>m_slaveIdList;//从站列表
    int m_pollIndex;//当前从站的索引
    //心跳探测
    int m_heartbeatIndex;//当前心跳所属从站的索引
    QMap<int,int>m_slaveHeartbeatFailCout;//从站和对应的心跳失败次数
    const int MAX_Heartbeat_Fail=3;//最大心跳重试次数
      QMap<int, bool> m_slaveOnline;          // 从站地址 -> 是否在线
    //功能函数
    QByteArray bulidModbusRequest03(int slaveAddr,int StartAddr,int numRegs);
    uint16_t crc16_modbus(const uint8_t* data, uint16_t length);//CRC校验
    QByteArray bulidModbusRequest06(int slaveaddr,int regaddr,int value);
    void processFrame(const QByteArray&frame);//解析报文
    void onReadyRead();
    void enqueueRequest(const ModbusRequest &req);//队列函数
    void processNext();//发送下一个队列请求函数
    void sendRawRequest(const QByteArray &frame, const ModbusRequest &req);//向串口发送报文请求,并且更新请求数据
    void handleHeartbeatFailure(int slaveId);//离线心跳探测
    void resetHeartbeatCounter(int slaveId);//重置计数器且恢复在线模式
};

#endif // MODBUSWORKER_H
