#include "modbusworker.h"
#include<QDebug>


ModbusWorker::ModbusWorker(QObject*parent):QObject(parent)
{
    //在工作类中创建串口对象,不然如果在主线程里创建对象,会导致线程亲和性出错
    serial=new QSerialPort(this);
    timeoutTimer=new QTimer(this);//超时重连定时器
    timeoutTimer->setSingleShot(true);
    waitingforresponse=false;
    retryCount=0;
    polltimer=new QTimer(this);//轮询定时器
    pollInterval=0;

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

void ModbusWorker::doReadRequest(int slaveaddr, int startaddr, int numregs)
{
    if(!serial->isOpen()){
        emit error("串口未打开");
        return;}
    if(waitingforresponse){
        emit this->error("正在等待上次响应");
        return;
    }
    //构建请求报文(虚拟串口设置从站地址1,功能码03,读取1个寄存器,起始寄存器地址为0)
    lastRequest=bulidModbusRequest03(slaveaddr,startaddr,numregs);
    serial->write(lastRequest);
 //启动超时重启
    waitingforresponse=true;
    retryCount=0;
     timeoutTimer->start(500);//超时时间为500ms单次
    //调试
    qDebug()<<"发送的寄存器请求为:"<<lastRequest.toHex();
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
    if(!serial->isOpen()){
        emit error("串口未打开");
        return;}
    if(waitingforresponse){
      emit this->error("正在等待上次响应");
        return;
    }
    //构建请求,从站地址此时被默认为1
    lastRequest=bulidModbusRequest06(salveaddr,regaddr,value);
  serial->write(lastRequest);
  waitingforresponse=true;
  retryCount=0;
   timeoutTimer->start(500);//超时时间为500ms单次
    qDebug()<<"06请求为"<<lastRequest.toHex();
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
          //解析这一帧,校验,更新界面
          processFrame(frame);
      }
  }
  void ModbusWorker::processFrame(const QByteArray &frame)
  {
      uint8_t func=static_cast<uint8_t>(frame[1]);
      //CRC校验
      QByteArray withoutCRC=frame.left(frame.size()-2);
      uint16_t calCRC=crc16_modbus(reinterpret_cast< const uint8_t*>(withoutCRC.constData()),withoutCRC.size());
      uint16_t recvCRC=static_cast<uint8_t>(frame[frame.size()-2])|(static_cast<uint8_t>(frame[frame.size()-1])<<8);
      if(calCRC!=recvCRC){
       emit this->error("CRC校验失败");
          return;
      }
      //--------------------根据功能码进行业务处理逻辑------------------------------------------------------
      //06
      if(func==0x06){
          waitingforresponse=false;//停止超时,已收到有效响应
          timeoutTimer->stop();
          emit this->writeCompleted(true,"写入成功");
      }
      else if(func==0x03){
          //假设读取1个寄存器
          if(frame.size()>=7 && static_cast<uint8_t>(frame[2])==2){
              int value=(static_cast<uint8_t>(frame[3])<<8)|static_cast<uint8_t>(frame[4]);
              waitingforresponse=false;//停止超时,已收到有效响应
              timeoutTimer->stop();
              emit this->readCompleted(value);
          }else{
              emit this->error("03响应格式错误");
          }
      }
      else if(func==0x83||func==0x85){
          waitingforresponse=false;
          timeoutTimer->stop();
          uint8_t errocode=static_cast<uint8_t>(frame[2]);
         emit this->error(QString("modbus异常,功能码%1,异常码%2").arg(func,0,16).arg(errocode));
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
        //如果收到正常响应了,WFR应该为false
        if(!waitingforresponse)return;
        retryCount++;
        if(retryCount<maxRetries){
            emit this->error("响应超时,尝试第%1次重连");
            serial->write(lastRequest);
            timeoutTimer->start(500);

        }else{
            waitingforresponse=false;
           emit this->error("响应请求失败,请重新发送请求");
        }//可以在这加入停止轮询,下一个版本加入
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
    if(waitingforresponse)
    {
        qDebug()<<"轮询跳过";
                return;
    }
    if(!serial->isOpen()){
        qDebug()<<"轮询停止,串口为打开";
        stopPolling();
        return;
    }
    doReadRequest(1,0,1);
}



























































