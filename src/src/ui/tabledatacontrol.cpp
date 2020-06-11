/**
 * @copyright 2020-2020 Uniontech Technology Co., Ltd.
 *
 * @file tabledatacontrol.cpp
 *
 * @brief 表格数据管理类
 *
 * @date 2020-06-10 17:56
 *
 * Author: zhaoyue  <zhaoyue@uniontech.com>
 *
 * Maintainer: zhaoyue  <zhaoyue@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tabledatacontrol.h"
#include <QDebug>
#include <QScrollBar>
#include <QMouseEvent>
#include <QHeaderView>
#include <QModelIndex>
#include <QJsonObject>
#include <QDateTime>
#include <QProcess>
#include <QThread>
#include <QDesktopServices>
//#include <QSound>

#include "../database/dbinstance.h"
#include "global.h"
#include "../aria2/aria2rpcinterface.h"
#include "tableModel.h"
#include "headerView.h"
#include "itemDelegate.h"
#include "settings.h"
#include "topButton.h"
#include "tableView.h"

using namespace Global;

tableDataControl::tableDataControl(TableView* pTableView, QObject *parent) :
    QObject(parent),
    m_pTableView(pTableView)
{
}


void tableDataControl::aria2MethodAdd(QJsonObject &json, QString &searchContent)
{
    QString id = json.value("id").toString();
    QString gId = json.value("result").toString();

    DataItem *finddata = m_pTableView->getTableModel()->find(id);

    if(finddata != nullptr) {
        finddata->gid = gId;
        finddata->taskId = id;
        QDateTime finish_time = QDateTime::fromString("", "yyyy-MM-dd hh:mm:ss");
        S_Task_Status get_status;
        S_Task_Status downloadStatus(finddata->taskId,
                                     Global::Status::Active,
                                     QDateTime::currentDateTime(),
                                     finddata->completedLength,
                                     finddata->speed,
                                     finddata->totalLength,
                                     finddata->percent,
                                     finddata->total,
                                     finish_time);


        S_Task_Status task;
        DBInstance::getTaskStatusById(finddata->taskId, task);
        if(task.m_taskId != "") {
            DBInstance::updateTaskStatusById(downloadStatus);
        } else {
            DBInstance::addTaskStatus(downloadStatus);
        }
        finddata->status = Global::Status::Active;
    } else {
        // 获取下载信息
        // aria2c->tellStatus(gId, gId);
        Aria2RPCInterface::Instance()->getFiles(gId, id);
        DataItem *data = new DataItem;
        data->taskId = id;
        data->gid = gId;
        data->Ischecked = 0;
        QDateTime time = QDateTime::currentDateTime();
        data->createTime = time.toString("yyyy-MM-dd hh:mm:ss");

        S_Task getTaskInfo;
        DBInstance::getTaskByID(id, getTaskInfo);
        S_Task task;
        if(getTaskInfo.m_taskId != "") {
            task = S_Task(getTaskInfo.m_taskId,
                          gId,
                          0,
                          getTaskInfo.m_url,
                          getTaskInfo.m_downloadPath,
                          getTaskInfo.m_downloadFilename,
                          time);
            DBInstance::updateTaskByID(task);
            data->fileName = getTaskInfo.m_downloadFilename;
        } else {
            task = S_Task(id, gId, 0, "", "", "Unknow", time);
            DBInstance::addTask(task);
        }
        m_pTableView->getTableModel()->append(data);
        if((searchContent != "") && !data->fileName.contains(searchContent)) {
            TableModel *dtModel = m_pTableView->getTableModel();
            m_pTableView->setRowHidden(dtModel->rowCount(QModelIndex()), true);
        }
    }
}

void tableDataControl::aria2MethodStatusChanged(QJsonObject &json, int iCurrentRow, QString &searchContent)
{
    QJsonObject result = json.value("result").toObject();
    QJsonObject bittorrent = result.value("bittorrent").toObject();
    QString     mode;
    QString     bittorrent_name;
    QString     taskId = json.value("id").toString();
    QString     bittorrent_dir = "";

    if(!bittorrent.isEmpty()) {
        mode = bittorrent.value("mode").toString();
        if(mode == "multi") {
            bittorrent_dir = result.value("dir").toString();
        }
        QJsonObject btInfo = bittorrent.value("info").toObject();
        bittorrent_name = btInfo.value("name").toString();
        QString infoHash = result.value("infoHash").toString();
        S_Url_Info tbUrlInfo;
        S_Url_Info getUrlInfo;
        DBInstance::getUrlById(taskId, getUrlInfo);
        if(getUrlInfo.m_taskId != "") {
            if(getUrlInfo.m_infoHash.isEmpty()) {
                S_Url_Info *url_info = new S_Url_Info(getUrlInfo.m_taskId,
                                                      getUrlInfo.m_url,
                                                      getUrlInfo.m_downloadType,
                                                      getUrlInfo.m_seedFile,
                                                      getUrlInfo.m_selectedNum,
                                                      infoHash);
                DBInstance::updateUrlById(*url_info);
            }
        }
    }
    QJsonArray files = result.value("files").toArray();

    QString filePath;
    QString fileUri;
    for(int i = 0; i < files.size(); ++i) {
        QJsonObject file = files[i].toObject();
        filePath = file.value("path").toString();
        QJsonArray uri = file.value("uris").toArray();
        for(int j = 0; j < uri.size(); ++j) {
            QJsonObject uriObject = uri[j].toObject();
            fileUri = uriObject.value("uri").toString();
        }
    }

    QString gId = result.value("gid").toString();

    long totalLength = result.value("totalLength").toString().toLong();         //
                                                                                //
                                                                                //
                                                                                //
                                                                                // 字节
    long completedLength = result.value("completedLength").toString().toLong(); //
                                                                                //
                                                                                //
                                                                                //
                                                                                // 字节
    long downloadSpeed = result.value("downloadSpeed").toString().toLong();     //
                                                                                //
                                                                                //
                                                                                //
                                                                                // 字节/每秒
    QString fileName = getFileName(filePath);
    QString statusStr = result.value("status").toString();

    int percent = 0;
    int status = 0;

    if((completedLength != 0) && (totalLength != 0)) {
        double tempPercent = completedLength * 100.0 / totalLength;
        percent = tempPercent;
        if((percent < 0) || (percent > 100)) {
            percent = 0;
        }
        if(completedLength == totalLength) {
            statusStr = "complete";
        }
    }

    if(statusStr == "active") {
        status = Global::Status::Active;
        QFileInfo fInfo(filePath);
        if(!fInfo.isFile()) {
            qDebug() << "文件不存在，";
        }
    } else if(statusStr == "waiting") {
        status = Global::Status::Waiting;
    } else if(statusStr == "paused") {
        status = Global::Status::Paused;
    } else if(statusStr == "error") {
        status = Global::Status::Error;
        dealNotificaitonSettings(statusStr, fileName);
    } else if(statusStr == "complete") {
        status = Global::Status::Complete;

        //下载文件为种子文件
        if(fileName.endsWith(".torrent")) {
            emit signalAutoDownloadBt(filePath);
        }

        //下载文件为磁链种子文件
        QString infoHash = result.value("infoHash").toString();
        if(filePath.startsWith("[METADATA]")) {
            QString dir = result.value("dir").toString();

            emit signalAutoDownloadBt(dir + "/" + infoHash + ".torrent");
            fileName = infoHash + ".torrent";
        }

        //
        dealNotificaitonSettings(statusStr, fileName);
        if(Settings::getInstance()->getDownloadFinishedOpenState()) {
            QDesktopServices::openUrl(QUrl(filePath, QUrl::TolerantMode));
        }
    } else if(statusStr == "removed") {
        status = Global::Status::Removed;
    }

    DataItem *data = m_pTableView->getTableModel()->find(taskId);
    if(data == nullptr) {
        return;
    }
    data->gid = gId;
    data->totalLength = formatFileSize(totalLength);
    data->completedLength = formatFileSize(completedLength);
    data->speed = (downloadSpeed != 0) ? formatDownloadSpeed(downloadSpeed) : "0kb/s";

    if(bittorrent.isEmpty()) {
        if(!fileName.isEmpty() && (data->fileName != fileName)) {
            data->fileName = fileName;
        }

        //                if(data->fileName==QObject::tr("Unknown"))
        //                {
        //                    data->fileName = (fileName.isEmpty()) ?
        // Global::UNKNOWN : fileName;
        //                }
        data->status = status;
    } else {
        // data->fileName = (bittorrent_name.isEmpty()) ? Global::UNKNOWN :
        // bittorrent_name;
        if(mode == "multi") {
            filePath = bittorrent_dir + "/" + bittorrent_name;
        }
        if((totalLength != 0) && (totalLength == completedLength)) {
            data->status = Complete;
            dealNotificaitonSettings("complete", filePath);
        } else {
            data->status = status;
        }

        fileUri = "";
    }
    data->percent = percent;
    data->total = totalLength;
    if(filePath != "") {
        data->savePath = filePath;
    } else {
        data->savePath = getDownloadSavepathFromConfig();
    }

    data->url = fileUri;
    data->time = "";

    if((totalLength != completedLength) && (totalLength != 0) &&
       (data->status == Global::Status::Active)) {
        QTime t(0, 0, 0);
        t = t.addSecs((totalLength - completedLength * 1.0) / downloadSpeed);
        data->time = t.toString("mm:ss");
    } else if((totalLength == 0) && (data->status == Global::Status::Active)) {
        data->time = ("--:--");
    } else {
        if(data->time == "") {
            data->time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        }

        //              updatetimer->stop();
    }
    S_Task task;
    S_Task getTask;
    DBInstance::getTaskByID(data->taskId, getTask);
    if(getTask.m_taskId != "") {
        if(getTask.m_url != "") {
            data->url = getTask.m_url;
        }
    }

    m_pTableView->update();
    m_pTableView->reset();
    S_Task_Status taskStatus;
    S_Task_Status getTaskStatus;
    DBInstance::getTaskStatusById(data->taskId, getTaskStatus);

    QDateTime get_time = QDateTime::fromString(data->time, "yyyy-MM-dd hh:mm:ss");
    S_Task_Status *save_task_status = new S_Task_Status(data->taskId,
                                                        data->status,
                                                        get_time,
                                                        data->completedLength,
                                                        data->speed,
                                                        data->totalLength,
                                                        data->percent,
                                                        data->total,
                                                        get_time);
    if(getTaskStatus.m_taskId == "") {
        DBInstance::addTaskStatus(*save_task_status);
    } else {
        if((getTaskStatus.m_downloadStatus != data->status) || (getTaskStatus.m_percent != data->speed)) {
            DBInstance::updateTaskStatusById(*save_task_status);
        }
    }
    m_pTableView->refreshTableView(iCurrentRow);
    if((data->status == Complete) && (searchContent != "")) {
        searchEditTextChanged(searchContent);
    }
}

void tableDataControl::aria2MethodShutdown(QJsonObject &json)
{
    QString result = json.value("result").toString();

    if(result == "OK") {
        // m_bShutdownOk = true;
        qDebug() << "close downloadmanager";
        //m_pTableView->close();
        DApplication::exit();
    }
}

void tableDataControl::aria2MethodGetFiles(QJsonObject &json, int iCurrentRow)
{
    QString   id = json.value("id").toString();
    DataItem *data = m_pTableView->getTableModel()->find(id);

    if(data == nullptr) { // id等于gid
        data = new DataItem();
        QJsonArray  ja = json.value("result").toArray();
        QJsonObject jo = ja.at(0).toObject();
        data->totalLength = jo.value("length").toString().toLong(); // 文件大小
        data->savePath = jo.value("path").toString();               //下载路径，带文件名
        data->fileName = data->savePath.mid(data->savePath.lastIndexOf('/') + 1);
        QJsonArray uris = jo.value("uris").toArray();
        data->url = uris.at(0).toObject().value("uri").toString();  //下载链接
        data->taskId = id;
        m_pTableView->getTableModel()->append(data);
    }
    m_pTableView->reset();
    m_pTableView->refreshTableView(iCurrentRow);
}

void tableDataControl::aria2MethodUnpause(QJsonObject &json, int iCurrentRow)
{
    QString gId = json.value("result").toString();
    QString taskId = json.value("id").toString();

    DataItem *data = m_pTableView->getTableModel()->find(taskId);

    if(data != nullptr) {
        data->status = Global::Status::Active;
        m_pTableView->refreshTableView(iCurrentRow);
    }
}

void tableDataControl::aria2MethodForceRemove(QJsonObject &json)
{
    QString id = json.value("id").toString();

    if(id.startsWith("REDOWNLOAD_")) { // 重新下载前的移除完成后
        QStringList sp = id.split("_");
        QString     taskId = sp.at(2);
        int rd = sp.at(1).toInt();
        QThread::msleep(500);
        emit signalRedownload(taskId, rd);
    }
}

void tableDataControl::saveDataBeforeClose()
{
    QList<DataItem *> dataList = m_pTableView->getTableModel()->dataList();
    QList<DelDataItem *> recyclelist = m_pTableView->getTableModel()->recyleList();

    if(recyclelist.size() > 0) {
        for(int j = 0; j < recyclelist.size(); j++) {
            DelDataItem *del_data = recyclelist.at(j);
            QDateTime    deltime = QDateTime::fromString(del_data->deleteTime, "yyyy-MM-dd hh:mm:ss");
            S_Task task(del_data->taskId, del_data->gid, 0, del_data->url, del_data->savePath,
                        del_data->fileName, deltime);

            DBInstance::updateTaskByID(task);
        }
    }
    if(dataList.size() > 0) {
        for(int i = 0; i < dataList.size(); i++) {
            DataItem *data = dataList.at(i);
            QDateTime time = QDateTime::fromString(data->createTime, "yyyy-MM-dd hh:mm:ss");


            S_Task task(data->taskId, data->gid, 0, data->url, data->savePath,
                        data->fileName, time);

            DBInstance::updateTaskByID(task);
            QDateTime finish_time;
            if(data->status == Global::Status::Complete) {
                finish_time = QDateTime::fromString(data->time, "yyyy-MM-dd hh:mm:ss");
            } else {
                finish_time = QDateTime::currentDateTime();
            }
            S_Task_Status get_status;
            int status;
            if((data->status == Global::Status::Complete) || (data->status == Global::Status::Removed)) {
                status = data->status;
            } else {
                status = Global::Status::Lastincomplete;
            }

            S_Task_Status download_status(data->taskId,
                                          status,
                                          finish_time,
                                          data->completedLength,
                                          data->speed,
                                          data->totalLength,
                                          data->percent,
                                          data->total,
                                          finish_time);

            if(DBInstance::getTaskStatusById(data->taskId, get_status) != false) {
                DBInstance::updateTaskStatusById(download_status);
            } else {
                DBInstance::addTaskStatus(download_status);
            }
        }
    }
}

QString tableDataControl::getFileName(const QString &url)
{
    return QString(url).right(url.length() - url.lastIndexOf('/') - 1);
}

void tableDataControl::dealNotificaitonSettings(QString statusStr, QString fileName)
{
    // 获取免打扰模式值
    QVariant undisturbed_mode_switchbutton = Settings::getInstance()->m_pSettings->getOption(
        "basic.select_multiple.undisturbed_mode_switchbutton");

    if(undisturbed_mode_switchbutton.toBool()) {
        bool topStatus = m_pTableView->isTopLevel();
        bool maxStatus = m_pTableView->isMaximized();
        if(topStatus && maxStatus) {
            return;
        }
    }

    bool afterDownloadPlayTone = Settings::getInstance()->getDownloadFinishedPlayToneState();
    if(afterDownloadPlayTone) {
        //QSound::play(":/resources/wav/downloadfinish.wav");
    } else {
        qDebug() << " not in select down load finsh wav" << endl;
    }

    bool downloadInfoNotify = Settings::getInstance()->getDownloadInfoSystemNotifyState();
    if(downloadInfoNotify) {
        QProcess *p = new QProcess;
        QString   showInfo;
        if(statusStr == "error") {
            showInfo = fileName + tr(" download failed, network error");
        } else {
            showInfo = fileName + tr(" download finished");
        }
        p->start("notify-send", QStringList() << showInfo);
        p->waitForStarted();
        p->waitForFinished();
    }
}

QString tableDataControl::formatFileSize(long size)
{
    QString result = "";

    if(size < 1024) {
        result = QString::number(size) + "B";
    } else if(size / 1024 < 1024) {
        result = QString::number(size * 1.0 / 1024, 'r', 1) + "KB";
    } else if(size / 1024 / 1024 < 1024) {
        result = QString::number(size * 1.0 / 1024 / 1024, 'r', 1) + "MB";
    } else if(size / 1024 / 1024 / 1024 < 1024) {
        result = QString::number(size * 1.0 / 1024 / 1024 / 1024, 'r', 1) + "GB";
    }

    return result;
}

QString tableDataControl::getDownloadSavepathFromConfig()
{
    return Settings::getInstance()->getDownloadSavePath();;
}

QString tableDataControl::formatDownloadSpeed(long size)
{
    QString result = "";

    if(size < 1024) {
        result = QString::number(size) + " B/s";
    } else if(size / 1024 < 1024) {
        result = QString::number(size * 1.0 / 1024, 'r', 1) + " KB/s";
    } else if(size / 1024 / 1024 < 1024) {
        result = QString::number(size * 1.0 / 1024 / 1024, 'r', 1) + " MB/s";
    } else if(size / 1024 / 1024 / 1024 < 1024) {
        result = QString::number(size * 1.0 / 1024 / 1024 / 1024, 'r', 1) + " GB/s";
    }

    return result;
}

bool tableDataControl::checkFileExist(QString &filePath)
{
    QFileInfo fInfo(filePath);

    return fInfo.isFile();
}

void tableDataControl::searchEditTextChanged(QString text)
{
    TableModel *pModel = m_pTableView->getTableModel();

    if(text == "") {
        for(int i = 0; i < pModel->rowCount(QModelIndex()); i++) {
            m_pTableView->setRowHidden(i, false);
            pModel->setData(pModel->index(i, 0), false, TableModel::Ischecked);
        }
    } else {
        for(int i = 0; i < pModel->rowCount(QModelIndex()); i++) {
            m_pTableView->setRowHidden(i, false);
            QString fileName = pModel->data(pModel->index(i, 1), TableModel::FileName).toString();
            if(!fileName.contains(text, Qt::CaseInsensitive)) {
                m_pTableView->setRowHidden(i, true);
            }
            pModel->setData(pModel->index(i, 0), false, TableModel::Ischecked);
        }
    }
    m_pTableView->reset();
}

void tableDataControl::initDataItem(Global::DataItem *data, const S_Task &tbTask)
{
    data->gid = tbTask.m_gid;
    data->url = tbTask.m_url;
    data->time = "0";
    data->speed = "0kb/s";
    data->taskId = tbTask.m_taskId;
    S_Task_Status taskStatus;
    DBInstance::getTaskStatusById(data->taskId, taskStatus);
    if(taskStatus.m_taskId != "") {
        data->percent = taskStatus.m_percent;
        data->fileName = tbTask.m_downloadFilename;
        data->savePath = tbTask.m_downloadPath;
        data->Ischecked = 0;
        data->totalLength = taskStatus.m_totalLength;
        data->completedLength = taskStatus.m_compeletedLength;
        if(taskStatus.m_downloadStatus == Global::Status::Active) {
            data->status = Global::Status::Lastincomplete;
        } else {
            data->status = taskStatus.m_downloadStatus;
        }
        data->total = taskStatus.m_totalFromSource;
        if(data->status == Global::Status::Complete) {
            data->time = taskStatus.m_modifyTime.toString("yyyy-MM-dd hh:mm:ss");
        }
    }
}

void tableDataControl::initDelDataItem(Global::DataItem* data, Global::DelDataItem *delData)
{
    S_Task_Status taskStatus;
    DBInstance::getTaskStatusById(data->taskId, taskStatus);
    delData->taskId = data->taskId;
    delData->gid = data->gid;
    delData->url = data->url;
    delData->status = data->status;
    delData->fileName = data->fileName;
    delData->savePath = data->savePath;
    delData->deleteTime = taskStatus.m_modifyTime.toString("yyyy-MM-dd hh:mm:ss");
    delData->totalLength = data->totalLength;
    delData->completedLength = data->completedLength;
    delData->finishTime = taskStatus.m_finishTime.toString("yyyy-MM-dd hh:mm:ss");
}