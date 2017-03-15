#ifndef QTS3_H
#define QTS3_H

#include <functional>

#include <QtCore>
#include <QtNetwork>

#include "qpm.h"

QPM_BEGIN_NAMESPACE(com, github, msorvig, s3)

class QtS3Private;
class QtS3;
class QtS3ReplyPrivate;
template <typename T>
class QtS3Reply;

class QtS3
{
public:
    QtS3(const QString &accessKeyId, const QString &secretAccessKey);
    QtS3(std::function<QByteArray()> accessKeyIdProvider,
         std::function<QByteArray()> secretAccessKeyProvider);

    QtS3Reply<QByteArray> location(const QByteArray &bucket);
    QtS3Reply<void> put(const QByteArray &bucket, const QString &path,
                        const QByteArray &content, const QStringList &headers = QStringList());
    QtS3Reply<bool> exists(const QByteArray &bucket, const QString &path);
    QtS3Reply<int> size(const QByteArray &bucket, const QString &path);
    QtS3Reply<QByteArray> get(const QByteArray &bucket, const QString &path);
    QtS3Reply<void> remove(const QByteArray &bucket, const QString &path);

    void clearCaches();
    QByteArray accessKeyId();
    QByteArray secretAccessKey();
private:
    QSharedPointer<QtS3Private> d;
};

class QtS3ReplyBase
{
public:
    enum S3Error {
        NoError,
        NetworkError,
        CredentialsError,
        BucketNameInvalidError,
        BucketNotFoundError,
        ObjectNameInvalidError,
        ObjectNotFoundError,
        GenereicS3Error,
        InternalSignatureError,
        InternalReplyInitializationError,
        InternalError,
        UnknownError,
    };

    QtS3ReplyBase(QtS3ReplyPrivate *replyPrivate);

    // error handling
    bool isSuccess();
    QNetworkReply::NetworkError networkError();
    QString networkErrorString();
    S3Error s3Error();
    QString s3ErrorString();
    QString anyErrorString();

    // verbatim reply as returned by AWS
    QByteArray replyData();

protected:
    QtS3ReplyPrivate *d; // ### should be explicitly shared.
};

template <typename T> class QtS3Reply : public QtS3ReplyBase
{
public:
    QtS3Reply(QtS3ReplyPrivate *replyPrivate);
    T value();
};

template <typename T>
QtS3Reply<T>::QtS3Reply(QtS3ReplyPrivate *replyPrivate)
    : QtS3ReplyBase(replyPrivate)
{

}

QPM_END_NAMESPACE(com, github, msorvig, s3)

#endif
