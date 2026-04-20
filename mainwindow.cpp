#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QtSerialPort/QSerialPortInfo>
#include <QDateTime>
#include <QStatusBar>
#include<QHeaderView>
#include<QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    serial(new QSerialPort(this)),
    timer(new QTimer(this)),
    recordcount(0),timeoutTimer(new QTimer(this)),waitingforresponse(0)
{
    ui->setupUi(this);
    setupm_ui();//执行创建界面
    updateSerialPorts();//执行扫描串口
    timeoutTimer->setSingleShot(true);//单次触发
    //信号槽
    connect(openButton,&QPushButton::clicked,this,&MainWindow::onOpenserial);
    connect(closeButton,&QPushButton::clicked,this,&MainWindow::onCloseserial);
        connect(startTimerButton,&QPushButton::clicked,this,&MainWindow::onStarTimer);
         connect(stopTimerButton,&QPushButton::clicked,this,&MainWindow::onStopTimer);
         connect(timer,&QTimer::timeout,this,&MainWindow::onSendData);
         connect(serial,&QSerialPort::readyRead,this,&MainWindow::onReadyRead);
connect(readModbusButton,&QPushButton::clicked,this,&MainWindow::on_readModbusRequest03);
connect(writeButton, &QPushButton::clicked, this, &MainWindow::on_writeButton_clicked);
connect(timeoutTimer,&QTimer::timeout,this,&MainWindow::On_timeout);
}

MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::setupm_ui(){
    //创建控件
    portComboBox=new QComboBox;//连接外部串口号下拉框
    baudComboBox=new QComboBox;//波特率下拉框
    baudComboBox->addItems({"9600","19200","115200"});
openButton=new QPushButton("打开");
closeButton=new QPushButton("关闭");
startTimerButton=new QPushButton("开始定时发送");//串口发送
stopTimerButton=new QPushButton("停止定时发送");
sendLineEdit=new QLineEdit;//用户输入
sendLineEdit->setPlaceholderText("输入串口号");//灰色提示符
tableWidget=new QTableWidget;
tableWidget->setColumnCount(3);
tableWidget->setHorizontalHeaderLabels({"序列号","收发数据","发送时间"});
tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);//自动填充
//MODBUS控件 03
readModbusButton= new QPushButton("读取寄存器");
ModbusValueLabel=new QLabel("未读取");
// 06
writeButton=new QPushButton("写入寄存器");
writeAddr=new QLineEdit;
writeAddr->setPlaceholderText("起始寄存器地址");
writeValue=new QLineEdit;
writeValue->setPlaceholderText("范围为0~65535");

//2.布局n
//第一行:串口号波特率的打开关闭按钮
QHBoxLayout*HLayout=new QHBoxLayout;
HLayout->addWidget(openButton);
HLayout->addWidget(closeButton);
HLayout->addWidget(new QLabel("串口号"));
HLayout->addWidget(portComboBox);
HLayout->addWidget(new QLabel("波特率:"));
HLayout->addWidget(baudComboBox);
//用户发送串口号布局
QHBoxLayout*sendLayout=new QHBoxLayout;
sendLayout->addWidget(new QLabel("发送内容:"));
sendLayout->addWidget(sendLineEdit);
//定时发送布局
QHBoxLayout*timerLayout=new QHBoxLayout;
timerLayout->addWidget(startTimerButton);
timerLayout->addWidget(stopTimerButton);
timerLayout->addStretch();
//modbus03布局
QHBoxLayout*modbusLayout=new QHBoxLayout;
modbusLayout->addWidget(readModbusButton);
modbusLayout->addWidget(new QLabel("寄存器数据为:"));
modbusLayout->addWidget(ModbusValueLabel);
//06
QHBoxLayout*writeLayout= new QHBoxLayout;
writeLayout->addWidget(new QLabel("写入16进制的寄存器数据:"));
writeLayout->addWidget(writeAddr);
writeLayout->addWidget(writeValue);
writeLayout->addWidget(writeButton);
//主布局
QVBoxLayout*mainLayout=new QVBoxLayout;
mainLayout->addLayout(HLayout);
mainLayout->addLayout(sendLayout);
mainLayout->addLayout(timerLayout);
mainLayout->addLayout(modbusLayout);
mainLayout->addLayout(writeLayout);
mainLayout->addWidget(tableWidget);
//中央控件
QWidget*centralWidget=new QWidget(this);//记得挂对象树
centralWidget->setLayout(mainLayout);
setCentralWidget(centralWidget);
   setWindowTitle("串口助手demo");
}

//CRC计算函数
uint16_t MainWindow::crc16_modbus(const uint8_t* data, uint16_t length)
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

//串口扫描
void MainWindow::updateSerialPorts(){
    portComboBox->clear();
    foreach(const QSerialPortInfo&info,QSerialPortInfo::availablePorts()){
        portComboBox->addItem(info.portName());
    }
    if(portComboBox->count()==0){
        statusBar()->showMessage("无可用串口");
    }}

//打开串口,用户配置规则和选择串口
    void MainWindow::onOpenserial(){//打开串口功能的槽函数
        QString Portname=portComboBox->currentText();
        if(Portname.isEmpty()){//检查串口是否可用
        statusBar()->showMessage("无可用的串口");
        return;//
    }
       serial->setPortName(Portname);
       serial->setBaudRate(baudComboBox->currentText().toInt());
       serial->setDataBits(QSerialPort::Data8);//8N1规则
       serial->setParity(QSerialPort::NoParity);
       serial->setStopBits(QSerialPort::OneStop);
       if(serial->open(QIODevice::ReadWrite)){      //指针传输给硬件读写规则
          statusBar()->showMessage(Portname+"打开成功");
          openButton->setEnabled(false);
          closeButton->setEnabled(true);
           startTimerButton->setEnabled(true);
       }else{
           statusBar()->showMessage("打开失败");
       }
}

//关闭串口
    void MainWindow::onCloseserial(){
        if(serial->isOpen()){
            serial->close();
        }
        if(timer->isActive()){
            timer->stop();
        }
        statusBar()->showMessage("串口已关闭");
        openButton->setEnabled(true);
        closeButton->setEnabled(false);
        startTimerButton->setEnabled(false);
        stopTimerButton->setEnabled(false);
    }
  //超时处理函数
    void MainWindow::On_timeout(){
        //如果收到正常响应了,WFR应该为false
        if(!waitingforresponse)return;
        retryCount++;
        if(retryCount<maxRetries){
            statusBar()->showMessage("响应超时,尝试第%1次重连");
            serial->write(lastRequest);
            timeoutTimer->start(500);

        }else{
            waitingforresponse=false;
            statusBar()->showMessage("响应请求失败,请重新发送请求");
        }//可以在这加入停止轮询,下一个版本加入
    }

//定时器打开槽
    void MainWindow::onStarTimer(){
        if(!serial->isOpen()){    //检查是否打开串口
            statusBar()->showMessage("请先打开串口");
            return;
        }
       timer->start(1000);//1s发一次
       startTimerButton->setEnabled(false);
       stopTimerButton->setEnabled(true);
       statusBar()->showMessage("定时发送启动,1s间隔");
    }
//定时器关闭槽
    void MainWindow::onStopTimer(){
        timer->stop();
        stopTimerButton->setEnabled(false);
        startTimerButton->setEnabled(true);
           statusBar()->showMessage("定时器已关闭");
    }

    //MODbus03 请求报文
    QByteArray MainWindow::bulidModbusRequest03(int slaveAddr, int StartAddr, int numRegs){
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

    //读取寄存器按钮的槽函数
    void MainWindow::on_readModbusRequest03(){
        if(!serial->isOpen()){
            statusBar()->showMessage("请先打开串口");
           return;
        }
        if(waitingforresponse){
            statusBar()->showMessage("正在等待上次响应");
            return;
        }

        //构建请求报文(虚拟串口设置从站地址1,功能码03,读取1个寄存器,起始寄存器地址为0)
        lastRequest=bulidModbusRequest03(1,0,1);
        serial->write(lastRequest);
     //启动超时重启
        waitingforresponse=true;
        retryCount=0;
         timeoutTimer->start(500);//超时时间为500ms单次
        //调试
        qDebug()<<"发送的寄存器请求为:"<<lastRequest.toHex();
    }


    //06写入单个寄存器请求
    QByteArray MainWindow::bulidModbusRequest06(int slaveaddr,int regaddr,int value){
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

  //写入的按钮的实现
    void MainWindow::on_writeButton_clicked(){
        //判断串口是否打开
        if(!serial->isOpen()){
            statusBar()->showMessage("请先打开串口");
        return;
        }
        if(waitingforresponse){
            statusBar()->showMessage("正在等待上次响应");
            return;
        }
        //寄存器首地址写入
        bool OKaddr;
        int regaddr=writeAddr->text().toInt(&OKaddr,16);
        if(!OKaddr){
statusBar()->showMessage("寄存器地址错误,请重新输入");
            return;
        }
        //寄存器值写入
        bool okVal;
        int value=writeValue->text().toInt(&okVal,10);
        if(!okVal){
            statusBar()->showMessage("寄存器值错误,返回");
        }
        //构建请求,从站地址此时被默认为1
        lastRequest=bulidModbusRequest06(1,regaddr,value);
      serial->write(lastRequest);
      waitingforresponse=true;
      retryCount=0;
       timeoutTimer->start(500);//超时时间为500ms单次
        qDebug()<<"06请求为"<<lastRequest.toHex();
    }
    //解析完整帧,以及更新UI,停止超时,CRC校验
    void MainWindow::processFrame(const QByteArray &frame)
    {
        uint8_t func=static_cast<uint8_t>(frame[1]);
        //CRC校验
        QByteArray withoutCRC=frame.left(frame.size()-2);
        uint16_t calCRC=crc16_modbus(reinterpret_cast< const uint8_t*>(withoutCRC.constData()),withoutCRC.size());
        uint16_t recvCRC=static_cast<uint8_t>(frame[frame.size()-2])|(static_cast<uint8_t>(frame[frame.size()-1])<<8);
        if(calCRC!=recvCRC){
            statusBar()->showMessage("CRC校验失败");
            return;
        }
        //--------------------根据功能码进行业务处理逻辑------------------------------------------------------
        //06
        if(func==0x06){
            waitingforresponse=false;//停止超时,已收到有效响应
            timeoutTimer->stop();
            statusBar()->showMessage("写入成功");
        }
        else if(func==0x03){
            //假设读取1个寄存器
            if(frame.size()>=7&&static_cast<uint8_t>(frame[2])==2){
                int value=(static_cast<uint8_t>(frame[3])<<8)|static_cast<uint8_t>(frame[4]);
                waitingforresponse=false;//停止超时,已收到有效响应
                timeoutTimer->stop();
                statusBar()->showMessage("读取成功");
                ModbusValueLabel->setText(QString::number(value));
                //表格控件记录
                int row=tableWidget->rowCount();
                tableWidget->insertRow(row);
                recordcount++;//计数器自增
                tableWidget->setItem(row,0,new QTableWidgetItem(QString::number(recordcount)));
                QString timerstr=QDateTime::currentDateTime().toString();
                tableWidget->setItem(row,1,new QTableWidgetItem(timerstr));
                tableWidget->setItem(row,2,new QTableWidgetItem(QString::number(value)));
                     statusBar() ->showMessage(QString("读取成功，值=%1").arg(value));
            }else{
                statusBar()->showMessage("03响应格式错误");
            }
        }
        else if(func==0x83||func==0x85){
            waitingforresponse=false;
            timeoutTimer->stop();
            uint8_t errocode=frame[2];
            statusBar()->showMessage(QString("modbus异常,功能码%1,异常码%2").arg(func,0,16).arg(errocode));
        }
    }
//数据发送槽,该功能暂时搁置
    void MainWindow::onSendData(){
       QString text0=sendLineEdit->text();
       if(text0.isEmpty()){
           statusBar()->showMessage("发送为空,本次跳过");
         return;
       }
       //发送数据
       serial->write(text0.toUtf8());
       //表格控件记录
       int row=tableWidget->rowCount();
       tableWidget->insertRow(row);
       recordcount++;//计数器自增
       tableWidget->setItem(row,0,new QTableWidgetItem(QString::number(recordcount)));
       tableWidget->setItem(row,1, new QTableWidgetItem(text0));
       QString timerstr=QDateTime::currentDateTime().toString();
       tableWidget->setItem(row,2,new QTableWidgetItem(timerstr));
    }
//数据接受槽,分包粘包处理,,仅仅处理完整1帧数据,不做UI更新和数据校验
    void MainWindow::onReadyRead()
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




