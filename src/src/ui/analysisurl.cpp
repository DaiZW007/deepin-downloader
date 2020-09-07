#include "analysisurl.h"
#include <QThread>
#include <QDebug>
#include <QMetaType>
#include <QMutex>

AnalysisUrl::AnalysisUrl(QObject *parent)
    : QObject(parent)
{
    //qRegisterMetaType<LinkInfo>("LinkInfo*");
}

AnalysisUrl::~AnalysisUrl()
{
}

void AnalysisUrl::setUrlList(QMap<QString, LinkInfo> list)
{
    if (list.isEmpty()) {
        return;
    }
    QMap<QString, LinkInfo>::iterator it = m_curAllUrl.begin();
    QMap<QString, LinkInfo>::iterator sameLink;
    for (; it != m_curAllUrl.end();) {
        if (list.contains(it.key())) {
            it.value().index = list.value(it.key()).index;
            *(m_curAllUrl.find(it.key())) = it.value();
            emit sendFinishedUrl(&it.value());
            it++;
        } else {
            stopWork(it.value().index);
            it = m_curAllUrl.erase(it);
        }
    }

    it = list.begin();

    for (; it != list.end(); it++) {
        if (m_curAllUrl.contains(it.key())) {
        } else {
            m_curAllUrl.insert(it.key(), it.value());
        }
    }
    it = m_curAllUrl.begin();

    static QMutex mutex;
    bool isLock = mutex.tryLock();
    if (isLock) {
        for (; it != m_curAllUrl.end(); it++) {
            if (it->state != LinkInfo::UrlState::Finished) {
                LinkInfo info;
                info.url = it.key();
                info.index = it.value().index;
                UrlThread *url = new UrlThread;
                QThread *m_thread = new QThread;
                m_workThread.insert(info.index, m_thread);
                url->moveToThread(m_thread);
                url->start(info);
                connect(m_thread, SIGNAL(started()), url, SLOT(begin()));
                connect(url, SIGNAL(sendFinishedUrl(LinkInfo)), this, SLOT(getLinkInfo(LinkInfo)));
                m_thread->start();
            }
        }
    }
    mutex.unlock();
}

void AnalysisUrl::getLinkInfo(LinkInfo linkInfo)
{
    emit sendFinishedUrl(&linkInfo);
}

void AnalysisUrl::stopWork(int index)
{
    QThread *thread = m_workThread.value(index);
    thread->requestInterruption();
    thread->quit();
    thread->wait();
    delete thread;
}