#include "qts3qnam_p.h"

#include <QtCore>
#include <QtNetwork/QNetworkReply>

QPM_BEGIN_NAMESPACE(com, github, msorvig, s3)

BlockingNetworkAccessManager::BlockingNetworkAccessManager(QObject *parent)
    :QNetworkAccessManager(parent)
{

}

QNetworkReply *BlockingNetworkAccessManager::syncGet(const QNetworkRequest &request)
{
    QNetworkReply *reply = get(request);

    QEventLoop loop;
    QObject::connect(this, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()));
    loop.exec();

    return reply;
}

QNetworkReply *SlottetNetworkAccessManager::sendCustomRequest_slot(const QNetworkRequest &request,
                                                                   const QByteArray &verb,
                                                                   QIODevice *data)
{
    return sendCustomRequest(request, verb, data);
}

// A thread-safe network access manager wrapper.
ThreadsafeBlockingNetworkAccesManager::ThreadsafeBlockingNetworkAccesManager()
{
    m_networkThread = new QThread;
    m_networkThread->start();
    m_networkAccessManager = new SlottetNetworkAccessManager;
    m_networkAccessManager->moveToThread(m_networkThread);
    m_requestCount = 0;
    m_cancellAll = false;
}

ThreadsafeBlockingNetworkAccesManager::~ThreadsafeBlockingNetworkAccesManager()
{
    delete m_networkAccessManager;
    m_networkThread->quit();
    m_networkThread->wait();
    delete m_networkThread;
}

// A synchronous, thread-safe sendCustomRequest.
QNetworkReply *ThreadsafeBlockingNetworkAccesManager::sendCustomRequest(
    const QNetworkRequest &request, const QByteArray &verb, QIODevice *data)
{
    // Maintain the active request count
    {
        QMutexLocker lock(&m_mutex);
        ++m_requestCount;
    }

    // Call sendCustomRequest on QNetworkAccessMaanger, on the network thread. Use a
    // BlockingQueuedConnection to get the returned reply object.
    QNetworkReply *reply = 0;
    QMetaObject::invokeMethod(m_networkAccessManager, "sendCustomRequest_slot",
                              Qt::BlockingQueuedConnection, Q_RETURN_ARG(QNetworkReply *, reply),
                              Q_ARG(QNetworkRequest, request), Q_ARG(QByteArray, verb),
                              Q_ARG(QIODevice *, data));

    // The reply should wake this thread when the request completes.
    // (this currently wakes all threads and could be optimized)
    connect(reply, SIGNAL(finished()), this, SLOT(wakeWaitingThreads()), Qt::DirectConnection);

    // Special case for HEAD requests: return on first metaDataChanged(). Needed to reduce
    // wait time - finished() is not signaled until S3 times out and clses the connection.
    const bool isHead = (verb == "HEAD");
    if (isHead)
        connect(reply, SIGNAL(metaDataChanged()), this, SLOT(wakeWaitingThreads()),
                Qt::DirectConnection);

    // Wait until the request completes, or is cancelled, or HEAD returns headers.
    {
        QMutexLocker lock(&m_mutex);
        while (!(reply->isFinished() || m_cancellAll || (isHead && reply->rawHeaderList().count() > 0))) {
            m_waitCompleted.wait(&m_mutex);
        }
    }

    // Abort the any network operation in progress if cancelling.
    if (m_cancellAll)
        reply->abort();

    // Maintain the active request count. Wake any waitAll waiters (lock
    // to avoid racing the wait() in waitForAll())
    {
        QMutexLocker lock(&m_mutex);
         --m_requestCount;
        if (m_requestCount == 0) {
            m_cancellAll = false;
            m_waitAll.wakeAll();
        }
    }

    return reply;
}

// Cancels all in-progress network operatikons. Sets a cancel state,
// which is in effect until the netowrk access manager is completely
// drained.
void ThreadsafeBlockingNetworkAccesManager::cancelAll()
{
    QMutexLocker lock(&m_mutex);
    if (m_requestCount == 0)
        return;

    m_cancellAll = true;
    m_waitCompleted.wakeAll();
}

// Wait until all in-progress netowork operations completes.
void ThreadsafeBlockingNetworkAccesManager::waitForAll()
{
    QMutexLocker lock(&m_mutex);
    m_waitAll.wait(&m_mutex);
}

int ThreadsafeBlockingNetworkAccesManager::pendingRequests()
{
    QMutexLocker lock(&m_mutex);
    return m_requestCount;
}

void ThreadsafeBlockingNetworkAccesManager::wakeWaitingThreads()
{
    QMutexLocker lock(&m_mutex);
    m_waitCompleted.wakeAll();
}

QPM_END_NAMESPACE(com, github, msorvig, s3)
