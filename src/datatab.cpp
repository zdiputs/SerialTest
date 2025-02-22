﻿#include "datatab.h"
#include "ui_datatab.h"

#include <QTimer>
#include <QTextCodec>
#include <QMessageBox>
#include <QClipboard>
#include <QFileDialog>
#include <QSerialPort>
#include <QDateTime>
#include <QDebug>

DataTab::DataTab(QByteArray* RxBuf, QByteArray* TxBuf, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataTab),
    rawReceivedData(RxBuf),
    rawSendedData(TxBuf)
{
    ui->setupUi(this);

    repeatTimer = new QTimer();
    RxSlider = ui->receivedEdit->verticalScrollBar();
    ui->dataTabSplitter->handle(1)->installEventFilter(this); // the id of the 1st visible handle is 1 rather than 0

    connect(ui->sendEdit, &QLineEdit::returnPressed, this, &DataTab::on_sendButton_clicked);
    connect(repeatTimer, &QTimer::timeout, this, &DataTab::on_sendButton_clicked);
    connect(RxSlider, &QScrollBar::valueChanged, this, &DataTab::onRxSliderValueChanged);
    connect(RxSlider, &QScrollBar::sliderMoved, this, &DataTab::onRxSliderMoved);

#ifdef Q_OS_ANDROID
    ui->data_flowControlBox->setVisible(false);
#endif
}

DataTab::~DataTab()
{
    delete ui;
}

void DataTab::initSettings()
{
    settings = MySettings::defaultSettings();
    loadPreference();

    connect(ui->receivedHexBox, &QCheckBox::clicked, this, &DataTab::saveDataPreference);
    connect(ui->receivedLatestBox, &QCheckBox::clicked, this, &DataTab::saveDataPreference);
    connect(ui->receivedRealtimeBox, &QCheckBox::clicked, this, &DataTab::saveDataPreference);
    connect(ui->sendedHexBox, &QCheckBox::clicked, this, &DataTab::saveDataPreference);
    connect(ui->data_suffixBox, &QGroupBox::clicked, this, &DataTab::saveDataPreference);
    connect(ui->data_suffixTypeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataTab::saveDataPreference);
    connect(ui->data_suffixEdit, &QLineEdit::editingFinished, this, &DataTab::saveDataPreference);
    connect(ui->data_repeatCheckBox, &QCheckBox::clicked, this, &DataTab::saveDataPreference);
    connect(ui->repeatDelayEdit, &QLineEdit::editingFinished, this, &DataTab::saveDataPreference);
    connect(ui->data_flowDTRBox, &QCheckBox::clicked, this, &DataTab::saveDataPreference);
    connect(ui->data_flowRTSBox, &QCheckBox::clicked, this, &DataTab::saveDataPreference);
}

bool DataTab::eventFilter(QObject *watched, QEvent *event)
{
    if(watched == ui->dataTabSplitter->handle(1))
    {
        // double click the handle to reset the size
        if(event->type() == QEvent::MouseButtonDblClick)
        {
            QList<int> newSizes = ui->dataTabSplitter->sizes(); // 2 elements
            newSizes[1] += newSizes[0];
            newSizes[0] = newSizes[1] * 0.5;
            newSizes[1] -= newSizes[0];
            ui->dataTabSplitter->setSizes(newSizes);
        }
        // save layout when mouse button is released
        else if(event->type() == QEvent::MouseButtonRelease)
        {
            QList<int> sizes = ui->dataTabSplitter->sizes(); // 2 elements
            double ratio = (double)sizes[0] / (sizes[0] + sizes[1]);
            settings->beginGroup("SerialTest_Data");
            settings->setValue("SplitRatio", ratio);
            settings->endGroup();
        }
    }
    return false;
}

void DataTab::showEvent(QShowEvent *ev)
{
    Q_UNUSED(ev)
    // ui->dataTabSplitter->sizes() will return 0 if the widgets are invisible
    settings->beginGroup("SerialTest_Data");
    QList<int> newSizes = ui->dataTabSplitter->sizes();
    double ratio = settings->value("SplitRatio", 0.5).toDouble();
    settings->endGroup();

    newSizes[1] += newSizes[0];
    newSizes[0] = newSizes[1] * ratio;
    newSizes[1] -= newSizes[0];

    ui->dataTabSplitter->setSizes(newSizes);
}

void DataTab::on_data_encodingSetButton_clicked()
{
    QTextCodec* newCodec;
    QComboBox* box;
    box = ui->data_encodingNameBox;
    newCodec = QTextCodec::codecForName(box->currentText().toLatin1());
    if(newCodec != nullptr)
    {
        if(box->itemText(box->currentIndex()) == box->currentText()) // existing text
            dataEncodingId = box->currentIndex();
        if(RxDecoder != nullptr)
            delete RxDecoder;
        dataCodec = newCodec;
        emit setDataCodec(dataCodec);
        RxDecoder = dataCodec->makeDecoder(); // clear state machine
        emit setPlotDecoder(dataCodec->makeDecoder());// clear state machine, standalone decoder for DataTab/PlotTab
        settings->beginGroup("SerialTest_Data");
        settings->setValue("Encoding_Name", ui->data_encodingNameBox->currentText());
        settings->endGroup();
    }
    else
    {
        QMessageBox::information(this, tr("Info"), ui->data_encodingNameBox->currentText() + " " + tr("is not a valid encoding."));
        box->setCurrentIndex(dataEncodingId);
    }
}

void DataTab::saveDataPreference()
{
    if(settings->group() != "")
        return;
    settings->beginGroup("SerialTest_Data");
    settings->setValue("Recv_Hex", ui->receivedHexBox->isChecked());
    settings->setValue("Recv_Latest", ui->receivedLatestBox->isChecked());
    settings->setValue("Recv_Realtime", ui->receivedRealtimeBox->isChecked());
    settings->setValue("Send_Hex", ui->sendedHexBox->isChecked());
    settings->setValue("Suffix_Enabled", ui->data_suffixBox->isChecked());
    settings->setValue("Suffix_Type", ui->data_suffixTypeBox->currentIndex());
    settings->setValue("Suffix_Context", ui->data_suffixEdit->text());
    settings->setValue("Repeat_Enabled", ui->data_repeatCheckBox->isChecked());
    settings->setValue("Repeat_Delay", ui->repeatDelayEdit->text());
    settings->setValue("Flow_DTR", ui->data_flowDTRBox->isChecked());
    settings->setValue("Flow_RTS", ui->data_flowRTSBox->isChecked());
    //Encoding_Name will not be saved there, because it need to be verified
    settings->endGroup();
}

// settings->setValue\((.+), ui->(.+)->currentIndex.+
// ui->$2->setCurrentIndex(settings->value($1).toInt());
// settings->setValue\((.+), ui->(.+)->text.+
// ui->$2->setText(settings->value($1).toString());

void DataTab::loadPreference()
{
    // default preferences are defined there
    settings->beginGroup("SerialTest_Data");
    ui->receivedHexBox->setChecked(settings->value("Recv_Hex", false).toBool());
    ui->receivedLatestBox->setChecked(settings->value("Recv_Latest", false).toBool());
    ui->receivedRealtimeBox->setChecked(settings->value("Recv_Realtime", true).toBool());
    ui->sendedHexBox->setChecked(settings->value("Send_Hex", false).toBool());
    ui->data_suffixBox->setChecked(settings->value("Suffix_Enabled", false).toBool());
    ui->data_suffixTypeBox->setCurrentIndex(settings->value("Suffix_Type", 2).toInt());
    ui->data_suffixEdit->setText(settings->value("Suffix_Context", "").toString());
    ui->data_repeatCheckBox->setChecked(settings->value("Repeat_Enabled", false).toBool());
    ui->repeatDelayEdit->setText(settings->value("Repeat_Delay", 1000).toString());
    ui->data_flowDTRBox->setChecked(settings->value("Flow_DTR", false).toBool());
    ui->data_flowRTSBox->setChecked(settings->value("Flow_RTS", false).toBool());
    ui->data_encodingNameBox->setCurrentText(settings->value("Encoding_Name", "UTF-8").toString());
    settings->endGroup();
    on_data_encodingSetButton_clicked();
}

void DataTab::on_data_suffixTypeBox_currentIndexChanged(int index)
{
    ui->data_suffixEdit->setVisible(index != 2 && index != 3);
    ui->data_suffixEdit->setPlaceholderText(tr("Suffix") + ((index == 1) ? "(Hex)" : ""));
}
void DataTab::onRxSliderValueChanged(int value)
{
    // qDebug() << "valueChanged" << value;
    currRxSliderPos = value;
}

void DataTab::onRxSliderMoved(int value)
{
    // slider is moved by user
    // qDebug() << "sliderMoved" << value;
    userRequiredRxSliderPos = value;
}

void DataTab::on_sendedHexBox_stateChanged(int arg1)
{
    isSendedDataHex = (arg1 == Qt::Checked);
    syncSendedEditWithData();
}

void DataTab::on_receivedHexBox_stateChanged(int arg1)
{
    isReceivedDataHex = (arg1 == Qt::Checked);
    syncReceivedEditWithData();
}

void DataTab::on_receivedClearButton_clicked()
{
    lastReceivedByte = '\0'; // anything but '\r'
    rawReceivedData->clear();
    emit setRxLabelText(tr("Rx") + ": 0");
    syncReceivedEditWithData();
}

void DataTab::on_sendedClearButton_clicked()
{
    rawSendedData->clear();
    emit setTxLabelText(tr("Tx") + ": 0");
    syncSendedEditWithData();
}

void DataTab::on_sendEdit_textChanged(const QString &arg1)
{
    Q_UNUSED(arg1);
    repeatTimer->stop();
    ui->data_repeatBox->setChecked(false);
}

void DataTab::on_data_repeatCheckBox_stateChanged(int arg1)
{
    if(arg1 == Qt::Checked)
    {
        repeatTimer->setInterval(ui->repeatDelayEdit->text().toInt());
        repeatTimer->start();
    }
    else
        repeatTimer->stop();
}

void DataTab::on_receivedCopyButton_clicked()
{
    QApplication::clipboard()->setText(ui->receivedEdit->toPlainText());
}

void DataTab::on_sendedCopyButton_clicked()
{
    QApplication::clipboard()->setText(ui->sendedEdit->toPlainText());
}

void DataTab::on_receivedExportButton_clicked()
{
    bool flag = true;
    QString fileName, selection;
    fileName = QFileDialog::getSaveFileName(this, tr("Export received data"), "recv_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss") + ".txt");
    if(fileName.isEmpty())
        return;
    QFile file(fileName);
    selection = ui->receivedEdit->textCursor().selectedText().replace(QChar(0x2029), '\n');
    if(selection.isEmpty())
    {
        flag &= file.open(QFile::WriteOnly);
        flag &= file.write(*rawReceivedData) != -1;
    }
    else
    {
        flag &= file.open(QFile::WriteOnly | QFile::Text);
        flag &= file.write(selection.replace(QChar(0x2029), '\n').toUtf8()) != -1;
    }
    file.close();
    QMessageBox::information(this, tr("Info"), flag ? tr("Successed!") : tr("Failed!"));
}

void DataTab::on_sendedExportButton_clicked()
{
    bool flag = true;
    QString fileName, selection;
    fileName = QFileDialog::getSaveFileName(this, tr("Export sended data"), "send_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss") + ".txt");
    if(fileName.isEmpty())
        return;
    QFile file(fileName);
    selection = ui->sendedEdit->textCursor().selectedText().replace(QChar(0x2029), '\n');
    if(selection.isEmpty())
    {
        flag &= file.open(QFile::WriteOnly);
        flag &= file.write(*rawSendedData) != -1;
    }
    else
    {
        flag &= file.open(QFile::WriteOnly | QFile::Text);
        flag &= file.write(selection.replace(QChar(0x2029), '\n').toUtf8()) != -1;
    }
    file.close();
    QMessageBox::information(this, tr("Info"), flag ? tr("Successed!") : tr("Failed!"));
}

void DataTab::on_receivedUpdateButton_clicked()
{
    syncReceivedEditWithData();
}

void DataTab::on_sendButton_clicked()
{
    QByteArray data;
    if(isSendedDataHex)
        data = QByteArray::fromHex(ui->sendEdit->text().toLatin1());
    else
        data = dataCodec->fromUnicode(ui->sendEdit->text());
    if(ui->data_suffixBox->isChecked())
    {
        if(ui->data_suffixTypeBox->currentIndex() == 0)
            data += dataCodec->fromUnicode(ui->data_suffixEdit->text());
        else if(ui->data_suffixTypeBox->currentIndex() == 1)
            data += QByteArray::fromHex(ui->data_suffixEdit->text().toLatin1());
        else if(ui->data_suffixTypeBox->currentIndex() == 2)
            data += "\r\n";
        else if(ui->data_suffixTypeBox->currentIndex() == 3)
            data += "\n";
    }

    emit send(data);
}

void DataTab::syncReceivedEditWithData()
{
    RxSlider->blockSignals(true);
    if(isReceivedDataHex)
        ui->receivedEdit->setPlainText(rawReceivedData->toHex(' ') + ' ');
    else
        // sync, use QTextCodec
        ui->receivedEdit->setPlainText(dataCodec->toUnicode(*rawReceivedData));
    RxSlider->blockSignals(false);
//    qDebug() << toHEX(*rawReceivedData);
}

void DataTab::syncSendedEditWithData()
{
    if(isSendedDataHex)
        ui->sendedEdit->setPlainText(rawSendedData->toHex(' ') + ' ');
    else
        ui->sendedEdit->setPlainText(dataCodec->toUnicode(*rawSendedData));
}

void DataTab::setIODevice(QIODevice *dev)
{
    IODevice = dev;
}

void DataTab::setFlowCtrl(bool isRTSValid, bool rts, bool dtr)
{
    ui->data_flowRTSBox->setVisible(isRTSValid);
    ui->data_flowRTSBox->setChecked(rts);
    ui->data_flowDTRBox->setChecked(dtr);
}

void DataTab::setRepeat(bool state)
{
    ui->data_repeatCheckBox->setChecked(state);
    on_data_repeatCheckBox_stateChanged(state);
}

bool DataTab::getRxRealtimeState()
{
    return ui->receivedRealtimeBox->isChecked();
}

// TODO:
// split sync process, add processEvents()
// void MainWindow::syncEditWithData()

void DataTab::appendReceivedData(const QByteArray& data)
{
    int cursorPos;
    int sliderPos;

    if(ui->receivedLatestBox->isChecked())
    {
        userRequiredRxSliderPos = RxSlider->maximum();
        RxSlider->setSliderPosition(RxSlider->maximum());
    }
    else
    {
        userRequiredRxSliderPos = currRxSliderPos;
        RxSlider->setSliderPosition(currRxSliderPos);
    }

    sliderPos = RxSlider->sliderPosition();

    cursorPos = ui->receivedEdit->textCursor().position();
    ui->receivedEdit->moveCursor(QTextCursor::End);
    if(isReceivedDataHex)
    {
        ui->receivedEdit->insertPlainText(data.toHex(' ') + ' ');
        hexCounter += data.length();
        // QPlainTextEdit is not good at handling long line
        // Seperate for better realtime receiving response
        if(hexCounter > 5000)
        {
            ui->receivedEdit->insertPlainText("\n");
            hexCounter = 0;
        }
    }
    else
    {
        // append, use QTextDecoder
        // if \r and \n are received seperatedly, the rawReceivedData will be fine, but the receivedEdit will have one more empty line
        // just ignore one of them
        if(lastReceivedByte == '\r' && !data.isEmpty() && *data.cbegin() == '\n')
            ui->receivedEdit->insertPlainText(RxDecoder->toUnicode(data.right(data.size() - 1)));
        else
            ui->receivedEdit->insertPlainText(RxDecoder->toUnicode(data));
        lastReceivedByte = *data.crbegin();
    }
    ui->receivedEdit->textCursor().setPosition(cursorPos);
    RxSlider->setSliderPosition(sliderPos);
}

#ifndef Q_OS_ANDROID
void DataTab::on_data_flowDTRBox_clicked(bool checked)
{
    QSerialPort* port = dynamic_cast<QSerialPort*>(IODevice);
    if(port != nullptr)
        port->setDataTerminalReady(checked);
}

void DataTab::on_data_flowRTSBox_clicked(bool checked)
{
    QSerialPort* port = dynamic_cast<QSerialPort*>(IODevice);
    if(port != nullptr && port->flowControl() != QSerialPort::HardwareControl)
        port->setRequestToSend(checked);
}
#endif

