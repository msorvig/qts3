#include "qts3.h"
#include "qts3_p.h"

QPM_BEGIN_NAMESPACE(com, github, msorvig, s3)

/*
    \class QtS3
    \brief The QtS3 class provides a synchronous API for accessing Amazon S3 content.

    The QtS3 class provides a functions for uploading and downloading S3 bucket content,
    as well as functons for querying metadata such as size and object existence.

    The API is synchronous (blocking) and thread-safe.
*/

/*!
    Constructs a QtS3 object with the given \a accessKeyId and \a secretAccessKey.
*/
QtS3::QtS3(const QString &accessKeyId, const QString &secretAccessKey)
    : d(new QtS3Private(accessKeyId.toLatin1(), secretAccessKey.toLatin1()))
{
}

/*!
    Constructs a QtS3 object with the given \a accessKeyIdProvider and \a secretAccessKeyProvider.

    Using this constructor allows QtS3 to load the secret access key on
    demand and keep it out of main memory when not used. The providers must
    be callable with the signature "QByteArray fn()".

    The secret access key is required when generating a signing key. QtS3 uses one
    signing key per accessed AWS region. The signing keys expire and are
    regenerated at regular intervals - \a secretAccessKeyProvider may be called
    during any call to the s3 operation functions. The key expiry time is
    several hours.
*/

QtS3::QtS3(std::function<QByteArray()> accessKeyIdProvider,
           std::function<QByteArray()> secretAccessKeyProvider)
: d(new QtS3Private(accessKeyIdProvider, secretAccessKeyProvider))
{
}


/*!
    Returns the region for the \a bucketName bucket. Example values are
    "us-east-1" and "eu-west-1".
*/
QtS3Reply<QByteArray> QtS3::location(const QByteArray &bucket)
{
    return QtS3Reply<QByteArray>(d->location(bucket));
}

/*!
    Uploads the given \a content to \a path in \a bucket. \a headers may
    contain optional request headers.
*/
QtS3Reply<void> QtS3::put(const QByteArray &bucket, const QString &path,
                          const QByteArray &content, const QStringList &headers)
{
    return QtS3Reply<void>(d->put(bucket, path, content, headers));
}

/*!
    Checks if the given \a path in \a bucket exists.
*/
QtS3Reply<bool> QtS3::exists(const QByteArray &bucket, const QString &path)
{
    return QtS3Reply<bool>(d->exists(bucket, path));
}

/*!
    Returns the size of the object at \a path in \a bucket. If the object
    does not exist the reply will have an error condition set.
*/
QtS3Reply<int> QtS3::size(const QByteArray &bucket, const QString &path)
{
    return QtS3Reply<int>(d->size(bucket, path));
}

/*!
    Downloads the content for the given \a path in \a bucket.
*/
QtS3Reply<QByteArray> QtS3::get(const QByteArray &bucket, const QString &path)
{
    return QtS3Reply<QByteArray>(d->get(bucket, path));
}

/*!
    Deletes the content for the given \a path in \a bucket.
*/
QtS3Reply<void> QtS3::remove(const QByteArray &bucket, const QString &path)
{
    return QtS3Reply<void>(d->remove(bucket, path));
}

/*!
    Clear internal caches such as the bucket region cache. Call this
    function if/when a bucket region changes.
*/
void QtS3::clearCaches()
{
    d->clearCaches();
}

/*!
    Returns the accessKeyId for the QtS3 object.
*/
QByteArray QtS3::accessKeyId()
{
    return d->accessKeyId();
}

/*!
    Returns the secretAccessKey for the QtS3 object.
*/
QByteArray QtS3::secretAccessKey()
{
    return d->secretAccessKey();
}

QtS3ReplyBase::QtS3ReplyBase(QtS3ReplyPrivate *replyPrivate) : d(replyPrivate) {}
// error handling
bool QtS3ReplyBase::isSuccess() { return d->isSuccess(); }

QNetworkReply::NetworkError QtS3ReplyBase::networkError() { return d->networkError(); }

QString QtS3ReplyBase::networkErrorString() { return d->networkErrorString(); }

QtS3ReplyBase::S3Error QtS3ReplyBase::s3Error() { return d->s3Error(); }

QString QtS3ReplyBase::s3ErrorString() { return d->s3ErrorString(); }

QString QtS3ReplyBase::anyErrorString() { return d->anyErrorString(); }

QByteArray QtS3ReplyBase::replyData() { return d->bytearrayValue(); }

template <> void QtS3Reply<void>::value() {}
template <> bool QtS3Reply<bool>::value() { return d->boolValue(); }
template <> int QtS3Reply<int>::value() { return d->intValue(); }
template <> QByteArray QtS3Reply<QByteArray>::value() { return d->bytearrayValue(); }

QPM_END_NAMESPACE(com, github, msorvig, s3)
