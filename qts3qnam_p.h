#ifndef QTS3QNAM_H
#define QTS3QNAM_H

#include <QtNetwork/QNetworkAccessManager>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include "qpm.h"

QPM_BEGIN_NAMESPACE(com, github, msorvig, s3)

class BlockingNetworkAccessManager : public QNetworkAccessManager
{
public:
    BlockingNetworkAccessManager(QObject *parent = 0);

    QNetworkReply *syncGet(const QNetworkRequest &request);
};

class SlottetNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
public slots:
    QNetworkReply *sendCustomRequest_slot(const QNetworkRequest &request, const QByteArray &verb,
                                          QIODevice *data = 0);
};

class ThreadsafeBlockingNetworkAccesManager : public QObject
{
    Q_OBJECT
public:
    ThreadsafeBlockingNetworkAccesManager();
    ~ThreadsafeBlockingNetworkAccesManager();
    QNetworkReply *sendCustomRequest(const QNetworkRequest &request, const QByteArray &verb,
                                     QIODevice *data = 0);
    void cancelAll();
    void waitForAll();
    int pendingRequests();

public slots:
    void wakeWaitingThreads();

private:
    QNetworkAccessManager *m_networkAccessManager;
    QThread *m_networkThread;
    QMutex m_mutex;
    QWaitCondition m_waitCompleted;
    QWaitCondition m_waitAll;
    int m_requestCount;
    bool m_cancellAll;
};

QPM_END_NAMESPACE(com, github, msorvig, s3)

#endif
