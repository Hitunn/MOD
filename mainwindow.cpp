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
    recordcount(0)
{
    ui->setupUi(this);
    setupm_ui();//执行创建界面
    updateSerialPorts();//执行扫描串口
    //创建线程和工作对象
    workerThread=new QThread(this);
    worker= new ModbusWorker;
    worker->moveToThread(workerThread);
    //信号槽
    //1.UI更新的信号槽
    connect(openButton,&QPushButton::clicked,this,&MainWindow::onOpenButtonClicked);
    connect(closeButton,&QPushButton::clicked,this,&MainWindow::onCloseButtonCicked);
    connect(readModbusButton,&QPushButton::clicked,this,&MainWindow::onReadButtonClicked);
    connect(writeButton, &QPushButton::clicked, this, &MainWindow::onwriteButtonClicked);
    //主线程跨工作线程信号槽
    connect(this,&MainWindow::sigOpenSerial,worker,&ModbusWorker::doOpenSerial);
    connect(this,&MainWindow::sigCloseSerial,worker,&ModbusWorker::doCloseSerial);
    connect(this,&MainWindow::sigReadRequest,worker,&ModbusWorker::doReadRequest);
    connect(this,&MainWindow::sigWriteRequest,worker,&ModbusWorker::doWriteRequest);
    //工作线程返回主线程信号槽
    connect(worker,&ModbusWorker::serialOpened,this,&MainWindow::onSeiralOpened);
    connect(worker,&ModbusWorker::serialClosed,this,&MainWindow::onSeiralClosed);
    connect(worker,&ModbusWorker::readCompleted,this,&MainWindow::onReadCompleted);
    connect(worker,&ModbusWorker::writeCompleted,this,&MainWindow::onWriteComplted);
    connect(worker,&ModbusWorker::error,this,&MainWindow::onError);
    //定时轮询
        connect(startTimerButton,&QPushButton::clicked,this,&MainWindow::onStartPollingButtonClicked);
         connect(stopTimerButton,&QPushButton::clicked,this,&MainWindow::onStopPollingButtonCicked);
         connect(this,&MainWindow::sigStarPolling,worker,&ModbusWorker::starPolling);
         connect(this,&MainWindow::sigStopPolling,worker,&ModbusWorker::stopPolling);
    workerThread->start();
}
MainWindow::~MainWindow()
{
    workerThread->quit();
    workerThread->wait();
    delete worker;
    delete workerThread;
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
    sendLineEdit->setPlaceholderText("输入串口号");//
    pollIntervalSpinbox=new QSpinBox;//轮旋指针
  pollIntervalSpinbox->setRange(100,1000);//间隔范围
  pollIntervalSpinbox->setSingleStep(100);//步长100
  pollIntervalSpinbox->setSuffix("ms");//单位
  pollIntervalSpinbox->setValue(100);//初始值100
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
    HLayout->addWidget(new QLabel("发送内容"));
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
    timerLayout->addWidget(pollIntervalSpinbox);
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
    setWindowTitle("ModBus主站模拟器");
}
//槽函数实现
void MainWindow::onOpenButtonClicked(){
    bool ok;
    QString port=portComboBox->currentText();
    int baud=baudComboBox->currentText().toInt(&ok,10);
    emit this->sigOpenSerial(port,baud);
}
void MainWindow::onCloseButtonCicked(){
    emit this->sigCloseSerial();
}
void MainWindow::onReadButtonClicked(){
    emit this->sigReadRequest(1,0,1);//由于需求太少了,这里暂时硬编码
}
void MainWindow::onwriteButtonClicked(){
    bool okaddr;
    bool okval;
    int addr=writeAddr->text().toInt(&okaddr,16);
    int value=writeValue->text().toInt(&okval,10);
    if(okaddr&&okval){
        emit this->sigWriteRequest(1,addr,value);
    }
    else{
        statusBar()->showMessage("输入格式有至少一个错误");
    }
}
void MainWindow::onSeiralOpened(bool success, const QString &msg)
{
    if(success){
        statusBar()->showMessage("打开成功");
    }
    statusBar()->showMessage(msg);
    openButton->setEnabled(!success);
    closeButton->setEnabled(success);
}
void MainWindow::onSeiralClosed()
{
    statusBar()->showMessage("串口已关闭");
    openButton->setEnabled(true);
    closeButton->setEnabled(false);
}
//轮询
void MainWindow::onStartPollingButtonClicked()
{
    int interval=pollIntervalSpinbox->value();
    emit this->sigStarPolling(interval);
}
void MainWindow::onStopPollingButtonCicked()
{
    emit this->sigStopPolling();
}
void MainWindow::onReadCompleted(int value)
{

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

}
void MainWindow::onWriteComplted(bool success, const QString &msg)
{
    statusBar()->showMessage(msg);
}
void MainWindow::onError(const QString &error){
    statusBar()->showMessage("错误为:"+error);
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



//数据发送槽,该功能暂时搁置
   /* void MainWindow::onSendData(){
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
    }*/





