#include "modbusworker.h"
#include<QDebug>

ModbusWorker::ModbusWorker(QObject*parent):QObject(parent),currentReqPriority(PRIO_POLL),m_busy(false)
{
    //在工作类中创建串口对象,不然如果在主线程里创建对象,会导致线程亲和性出错
    serial=new QSerialPort(this);
    timeoutTimer=new QTimer(this);//超时重连定时器
    timeoutTimer->setSingleShot(true);
    retryCount=0;
    polltimer=new QTimer(this);//轮询定时器
    pollInterval=0;
    heartbeatFailCout=0;
    isDeviceOnline=true;
    onlineHeartbeatInterval=5000;
    offlineHeartbeatInterval=10000;//离线探测频率
    m_pollIndex=0;//轮询从站索引
    m_heartbeatIndex=0;//心跳
    m_slaveIdList={1,2};//2个从站,实际可读配置
    for (int id : m_slaveIdList) {
        m_slaveOnline[id] = true;   // 初始都认为在线
    }
initSlaveHeartbeatTimers();
    //读取串口
    connect(serial,&QSerialPort::readyRead,this,&ModbusWorker::onReadyRead);
    //超时重连
    connect(timeoutTimer,&QTimer::timeout,this,&ModbusWorker::On_timeout);
    //轮询
    connect(polltimer,&QTimer::timeout,this,&ModbusWorker::onPollTimer);
}
ModbusWorker::~ModbusWorker(){
}
//传回主线程的槽函数
void ModbusWorker::doOpenSerial(const QString &portname, int baud){
    if(portname.isEmpty()){//检查串口是否可用
        emit this->error("无可用的串口");
        return;//
    }
    serial->setPortName(portname);
    serial->setBaudRate(baud);
    serial->setDataBits(QSerialPort::Data8);//8N1规则
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    bool ok=(serial->open(QIODevice::ReadWrite));//指针传输给硬件读写规则
    QString msg=ok?(portname+"打开成功"):"打开失败";
    heartbeatFailCout=0;
    isDeviceOnline=true;
    emit deviceOnline();
    emit this->serialOpened(ok,msg);
}
void ModbusWorker::doCloseSerial(){
    if(serial->isOpen()){
        serial->close();
    }
    if(timeoutTimer->isActive()){
        timeoutTimer->stop();
    }
    emit this->serialClosed();
}
//入队列函数
void ModbusWorker::enqueueRequest(const ModbusRequest &req)
{ //将请求入列
    m_requestQueue.enqueue(req);
    if(!m_busy){
        processNext();
        qDebug() << "入队请求, 当前队列大小:" << m_requestQueue.size();
    }
}
//下一个队列请求函数
void ModbusWorker::processNext()
{
    if(m_busy)return;
    if(m_requestQueue.isEmpty()) return;
     m_busy=true;
     //取出队列
     m_currentReq=m_requestQueue.dequeue();
     QByteArray frame;
        qDebug() << "processNext: 开始处理, m_busy=" << m_busy;
     if(m_currentReq.funcCode==0x03){
        frame=bulidModbusRequest03(m_currentReq.slaveId,m_currentReq.startAddr,m_currentReq.valueOrCount);

     }else if(m_currentReq.funcCode==0X06) {
         frame=bulidModbusRequest06(m_currentReq.slaveId,m_currentReq.startAddr,m_currentReq.valueOrCount);
     }else {
         //无效或未知功能码,跳过重新请求队列
         m_busy=false;
         processNext();
         return;

     }
   sendRawRequest(frame,m_currentReq);
}
void ModbusWorker::sendRawRequest(const QByteArray &frame, const ModbusRequest &req)
{
    if(!serial->isOpen()){
        emit error("串口未打开");
        m_busy = false;
              processNext();
              return;
    }
    //请求结构体更新
    lastRequest = frame;
     lastRequestHeartbeat = req.isHeartbeat;
     currentAllowRetry = req.allowRetry;
     m_busy = true;
     retryCount = 0;
     currentReqPriority = req.priority;
     timeoutTimer->start(500);
     //发送给串口
     serial->write(frame);
     qDebug() << "实际发送, 是否心跳:" << req.isHeartbeat;
     qDebug() << "发送请求:" << frame.toHex() << "从站:" << req.slaveId;
}
//初始化心跳定时器的槽函数
void ModbusWorker::initSlaveHeartbeatTimers(){
    for(int slaveId:m_slaveIdList){
        QTimer*timer=new QTimer(this);
        timer->setInterval(onlineHeartbeatInterval);
        //用lambda捕获循环连接每个从站的定时器
        connect(timer,&QTimer::timeout,this,[this,slaveId](){
            onSlaveHeartbeatTimeout(slaveId);
        });
        m_slaveHeartbeatTimers[slaveId]=timer;
        timer->start();

    }
}
//心跳超时函数
void ModbusWorker::onSlaveHeartbeatTimeout(int slaveId){
    if(!serial->isOpen())return;
      //防止与上次任何其他响应冲突,跳过本次心跳
        if(m_slaveIdList.isEmpty())return;
        doReadRequest(slaveId,0,1,true,true);//用第四个参数区分心跳请求和普通请求
        qDebug() <<"从站"<<QString::number(slaveId)<< "心跳定时器触发";

}
//心跳失败处理函数
void ModbusWorker::handleHeartbeatFailure(int slaveId)
{
    if(m_slaveHeartbeatTimers.contains(slaveId)){
        QTimer *timer = m_slaveHeartbeatTimers[slaveId];
           if (timer->interval() != offlineHeartbeatInterval) {
               timer->setInterval(offlineHeartbeatInterval);
               qDebug() << "从站" << slaveId << "离线慢频率";
           }
       }
    }

//重置计数器且恢复在线模式
void ModbusWorker::resetHeartbeatCounter(int slaveId)
{
    if(m_slaveHeartbeatTimers.contains(slaveId)){
        QTimer *timer = m_slaveHeartbeatTimers[slaveId];
           if (timer->interval() != onlineHeartbeatInterval) {
               timer->setInterval(onlineHeartbeatInterval);
               qDebug() << "从站" << slaveId << "恢复在线，心跳恢复快速模式";
           }
       }
    }

//MODbus03 请求报文
QByteArray ModbusWorker::bulidModbusRequest03(int slaveAddr, int StartAddr, int numRegs){
    QByteArray frame;
    frame.append(static_cast<char>(slaveAddr));
    frame.append(0x03);
    frame.append(static_cast<char>((StartAddr>>8)&0xFF));
    frame.append(static_cast<char>(StartAddr&0XFF));
    frame.append(static_cast<char>(numRegs>>8)&0xFF);
    frame.append(static_cast<char>(numRegs&0XFF));
    uint16_t crc=crc16_modbus(reinterpret_cast< const uint8_t*>(frame.constData()),frame.size());
    frame.append(static_cast<char>(crc&0XFF));
    frame.append(static_cast<char>(crc>>8)&0XFF);
    return frame;
}
//写请求,函数实现改为仅发送队列请求
void ModbusWorker::doReadRequest(int slaveaddr, int startaddr, int numregs,bool isHeartbeat,bool allowRetry)
{
    ReqPriority PRIO = isHeartbeat?PRIO_HEARTBEAT:(allowRetry?PRIO_MANUAL:PRIO_POLL);//判断请求优先级
    ModbusRequest req(slaveaddr,0x03,startaddr,numregs,isHeartbeat,allowRetry,PRIO);
    //调用队列函数
    enqueueRequest(req);
}

//06写入单个寄存器请求
QByteArray ModbusWorker::bulidModbusRequest06(int slaveaddr,int regaddr,int value){
    QByteArray frame;
    frame.append(static_cast<char>(slaveaddr));
    frame.append(0x06);
    //高位在前
    frame.append(static_cast<char>((regaddr>>8)&0XFF));
    frame.append(static_cast<char>(regaddr&0XFF));
    //写入值高位在前
    frame.append(static_cast<char>((value>>8)&0XFF));
    frame.append(static_cast<char>(value&0XFF));
    //计算CRC
    uint16_t crc=crc16_modbus(reinterpret_cast<const uint8_t*>(frame.constData()), frame.size());
    //低字节在前
    frame.append(static_cast<char>(crc&0XFF));
    frame.append(static_cast<char>((crc>>8)&0XFF));
    return frame;
}
void ModbusWorker::doWriteRequest(int salveaddr, int regaddr, int value)
{
    //写入请求默认允许抢占
    ModbusRequest req (salveaddr,0X06,regaddr,value,false,true,PRIO_MANUAL);
            enqueueRequest(req);
}
//数据接受槽,分包粘包处理,,仅仅处理完整1帧数据,不做UI更新和数据校验
void ModbusWorker::onReadyRead()
{
    QByteArray data=serial->readAll();//读取从站响应数据
    //健壮性
    if(data.isEmpty())return;
    qDebug()<<"寄存器数据为:"<<data.toHex(); //显示原始16进制数据,方便调试
    recvBuffer.append(data);//追加到缓冲区

    //分包粘包处理,while循环判断取出完整帧
    while(true){
        //至少有2字节才有功能码
        if(recvBuffer.size()<2)break;
        uint8_t func=static_cast<uint8_t>(recvBuffer[1]);
        int framelen;
        //根据功能码来判断1帧的长度
        if(func==03){
            if (recvBuffer.size()<3)break;
            uint8_t bytecount=static_cast<uint8_t>(recvBuffer[2]);
            framelen=3+bytecount+2;//功能码+首地址+字节数
        }
        else if(func==06){
            if(recvBuffer.size()<3)break;
            framelen=8;//)06功能码只能为8
        }
        else if (func==0x83||func==0x86){
            framelen=5;//异常码
        }
        else {
            recvBuffer.remove(0,1);
            continue;  //不支持的功能码,直接移除
        }
        //检测缓冲区是否构成一帧
        if(recvBuffer.size()<framelen)break;
        //取出判断后的完整1帧
        QByteArray frame=recvBuffer.left(framelen);//左移,有可能缓冲区粘包
        recvBuffer.remove(0,framelen);
        //解析这一帧,校验,
        processFrame(frame);
    }
}
void ModbusWorker::processFrame(const QByteArray &frame)
{
    uint8_t slaveId=static_cast<uint8_t>(frame[0]);
    uint8_t func=static_cast<uint8_t>(frame[1]);
    //CRC校验
    QByteArray withoutCRC=frame.left(frame.size()-2);
    uint16_t calCRC=crc16_modbus(reinterpret_cast< const uint8_t*>(withoutCRC.constData()),withoutCRC.size());
    uint16_t recvCRC=static_cast<uint8_t>(frame[frame.size()-2])|(static_cast<uint8_t>(frame[frame.size()-1])<<8);
    if(calCRC!=recvCRC){
        emit this->error("CRC校验失败");
        m_busy=false;
            processNext();
        return;
    }
    //只要收到任何正确响应都重置该从站的心跳失败计数
    if(func==0x03||func==0x06){
        m_slaveHeartbeatFailCout[slaveId]=0;
            m_slaveOnline[slaveId] = true;
            resetHeartbeatCounter(slaveId);
        qDebug() << QString("从站 %1 响应成功，心跳失败计数归零").arg(slaveId);
    }

    //--------------------根据功能码进行业务处理逻辑------------------------------------------------------
    //06
    if(func==0x06){
       m_busy=false;//停止超时,已收到有效响应
        timeoutTimer->stop();
        emit this->writeCompleted(true,"写入成功");
        m_busy = false;
        processNext();
    }
    else if(func==0x03){
        //假设读取1个寄存器
        if(frame.size()>=7 && static_cast<uint8_t>(frame[2])==2){
            int value=(static_cast<uint8_t>(frame[3])<<8)|static_cast<uint8_t>(frame[4]);
            m_busy=false;//停止超时,已收到有效响应
            timeoutTimer->stop();
            emit this->readCompleted(value);
            m_busy = false;
            processNext();
        }else{
            emit this->error("03响应格式错误");
            m_busy = false;
            processNext();
        }
    }
    else if(func==0x83||func==0x85){
       m_busy =false;
        timeoutTimer->stop();
        uint8_t errocode=static_cast<uint8_t>(frame[2]);
        emit this->error(QString("modbus异常,功能码%1,异常码%2").arg(func,0,16).arg(errocode));
        m_busy = false;
        processNext();
    }
}

//CRC计算函数
uint16_t ModbusWorker::crc16_modbus(const uint8_t* data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;   // Modbus 多项式
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

//超时处理函数
void ModbusWorker::On_timeout(){
    //如果收到正常响应了,busy标记应该是false
    if(!m_busy)return;
    //如果是轮询的重试请求直接跳过
    if (currentReqPriority == PRIO_POLL) {
            // 轮询请求超时：直接放弃，不重试
           m_busy = false;
               processNext();
            qDebug() << "轮询超时，已放弃";
            return;
        }
    retryCount++;
    if(retryCount<maxRetries){
        emit this->error(QString("响应超时,尝试第%1次重连").arg(retryCount));
        serial->write(lastRequest);
        timeoutTimer->start(500);
        qDebug()<<(QString("响应超时,尝试第%1次重连").arg(retryCount));
    }
    else {
        if(lastRequestHeartbeat){
            int slaveId=m_currentReq.slaveId;
            int fail=m_slaveHeartbeatFailCout.value(slaveId,0)+1;
            m_slaveHeartbeatFailCout[slaveId]=fail;
            qDebug()<<QString("从站%1心跳超时,连续失败次数为%2").arg(slaveId).arg(fail);
            if(fail>MAX_Heartbeat_Fail){
                if(m_slaveOnline.value(slaveId)){
                    m_slaveOnline[slaveId]=false;
                    handleHeartbeatFailure( slaveId);
                  qDebug() << QString("从站 %1 已离线").arg(slaveId);}
            }
        }
        else emit this->error("响应请求失败,请重新发送请求");
        m_busy = false;
        processNext();
    }
}
//轮询打开槽函数
void ModbusWorker::starPolling(int intervalMS){
    if(!serial->isOpen()){
        emit this->error("串口未打开,无法轮询,请打开串口");
        return;
    }
    if(polltimer->isActive())
        polltimer->stop();
    polltimer->start(intervalMS);
    qDebug()<<"轮询启动,间隔为:"<<intervalMS;
}
void ModbusWorker::stopPolling(){
    if(polltimer->isActive())
        polltimer->stop();
    qDebug()<<"轮询关闭";
}
//轮询发送请求槽函数
void ModbusWorker::onPollTimer()
{
    //避免在超时重发未完成之前发送新请求
    if(m_busy)
    {
        qDebug()<<"轮询跳过";
        return;
    }
    if(!serial->isOpen()){
        qDebug()<<"轮询停止,串口未打开";
        stopPolling();
        return;
    }
    int originIndex=m_pollIndex;
    while(true){

        int slaveId=m_slaveIdList[m_pollIndex];
        if(m_slaveOnline.value(slaveId)){
            doReadRequest(slaveId,0,1,false,false);
            m_pollIndex=(m_pollIndex+1)%m_slaveIdList.size();//自增取模,最大为size-1然后重新技术
            break;
        }else{
            m_pollIndex=(m_pollIndex+1)%m_slaveIdList.size();//
            if(m_pollIndex==originIndex){
                //此时说明已经遍历完了所有从站都不在线,
                qDebug()<<"所有从站全部离线";
                stopPolling();
                break;

            }
        }
    }
}


























































