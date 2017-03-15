#ifndef QTS3_P_H
#define QTS3_P_H

#include "qts3.h"
#include "qts3qnam_p.h"

#include <QLoggingCategory>
#include <QtNetwork>

#include <functional>

QPM_BEGIN_NAMESPACE(com, github, msorvig, s3)

Q_DECLARE_LOGGING_CATEGORY(qts3_API)
Q_DECLARE_LOGGING_CATEGORY(qts3_Internal)

class QtS3Private
{
public:
    QtS3Private();
    QtS3Private(QByteArray accessKeyId, QByteArray secretAccessKey);
    QtS3Private(std::function<QByteArray()> accessKeyIdProvider,
                std::function<QByteArray()> secretAccessKeyProvider);
    ~QtS3Private();

    std::function<QByteArray()> m_accessKeyIdProvider;
    std::function<QByteArray()> m_secretAccessKeyProvider;
    QByteArray m_service;
    ThreadsafeBlockingNetworkAccesManager *m_networkAccessManager;

    class S3KeyStruct
    {
    public:
        QDateTime timeStamp;
        QByteArray key;
    };
    QHash<QByteArray, S3KeyStruct> m_signingKeys;  // region -> key struct
    QReadWriteLock m_signingKeysLock;
    QHash<QByteArray, QByteArray> m_bucketRegions; // bucket name -> region
    QReadWriteLock m_bucketRegionsLock;

    static QByteArray hash(const QByteArray &data);
    static QByteArray sign(const QByteArray &key, const QByteArray &data);

    static QByteArray deriveSigningKey(const QByteArray &secretAccessKey,
                                       const QByteArray dateString, const QByteArray &region,
                                       const QByteArray &m_service);

    static QByteArray formatDate(const QDate &date);
    static QByteArray formatDateTime(const QDateTime &dateTime);
    static QByteArray formatHeaderNameValueList(const QMap<QByteArray, QByteArray> &headers);
    static QByteArray formatHeaderNameList(const QMap<QByteArray, QByteArray> &headers);
    static QByteArray createCanonicalQueryString(const QByteArray &queryString);
    static QMap<QByteArray, QByteArray> canonicalHeaders(const QHash<QByteArray, QByteArray> &headers);
    static QHash<QByteArray, QByteArray> parseHeaderList(const QStringList &headers);
    static QByteArray formatCanonicalRequest(const QByteArray &method, const QByteArray &url,
                                             const QByteArray &queryString,
                                             const QHash<QByteArray, QByteArray> &headers,
                                             const QByteArray &payloadHash);
    static QByteArray formatStringToSign(const QDateTime &timeStamp, const QByteArray &m_region,
                                         const QByteArray &m_service,
                                         const QByteArray &canonicalReqeustHash);
    static QByteArray
    formatAuthorizationHeader(const QByteArray &awsAccessKeyId, const QDateTime &timeStamp,
                              const QByteArray &m_region, const QByteArray &m_service,
                              const QByteArray &signedHeaders, const QByteArray &signature);

    static QByteArray signRequestData(const QHash<QByteArray, QByteArray> headers,
                                      const QByteArray &verb, const QByteArray &url,
                                      const QByteArray &queryString, const QByteArray &payload,
                                      const QByteArray &signingKey, const QDateTime &dateTime,
                                      const QByteArray &m_region, const QByteArray &m_service);
    static QByteArray
    createAuthorizationHeader(const QHash<QByteArray, QByteArray> headers, const QByteArray &verb,
                              const QByteArray &url, const QByteArray &queryString,
                              const QByteArray &payload, const QByteArray &accessKeyId,
                              const QByteArray &signingKey, const QDateTime &dateTime,
                              const QByteArray &m_region, const QByteArray &m_service);

    // Signing key management
    static bool checkGenerateSigningKey(QHash<QByteArray, QtS3Private::S3KeyStruct> *signingKeys,
                                        const QDateTime &now,
                                        std::function<QByteArray()> secretAccessKeyProvider,
                                        const QByteArray &m_region, const QByteArray &m_service);

    // QNetworkRequest creation and signing;
    static QHash<QByteArray, QByteArray> requestHeaders(const QNetworkRequest *request);
    static void setRequestAttributes(QNetworkRequest *request, const QUrl &url,
                                     const QHash<QByteArray, QByteArray> &headers,
                                     const QDateTime &timeStamp, const QByteArray &m_host);
    static void signRequest(QNetworkRequest *request, const QByteArray &verb,
                            const QByteArray &payload, const QByteArray accessKeyId,
                            const QByteArray &signingKey, const QDateTime &dateTime,
                            const QByteArray &m_region, const QByteArray &m_service);

    // Error handling
    static QHash<QByteArray, QByteArray> getErrorComponents(const QByteArray &errorString);
    static QByteArray getStringToSign(const QByteArray &errorString);
    static QByteArray getCanonicalRequest(const QByteArray &errorString);

    // Top-level stateful functions. These read object state and may/will modify it in a thread-safe way.
    void init();
    void checkGenerateS3SigningKey(const QByteArray &region);
    QNetworkRequest *createSignedRequest(const QByteArray &verb, const QUrl &url,
                                         const QHash<QByteArray, QByteArray> &headers,
                                         const QByteArray &host, const QByteArray &payload,
                                         const QByteArray &region);
    QNetworkReply *sendRequest(const QByteArray &verb, const QNetworkRequest &request,
                               const QByteArray &payload);
    QNetworkReply *sendS3Request(const QByteArray &bucketName, const QByteArray &verb,
                                 const QString &path, const QByteArray &queryString,
                                 const QByteArray &content, const QStringList &headers);

    bool checkBucketName(QtS3ReplyPrivate *s3Reply, const QByteArray &bucketName);
    bool checkPath(QtS3ReplyPrivate *s3Reply, const QByteArray &path);
    bool cacheBucketLocation(QtS3ReplyPrivate *s3Reply, const QByteArray &bucketName);
    void processNetworkReplyState(QtS3ReplyPrivate *s3Reply, QNetworkReply *networkReply);
    QtS3ReplyPrivate *location_impl(const QByteArray &bucketName);
    QtS3ReplyPrivate *processS3Request(const QByteArray &verb, const QByteArray &bucketName,
                                       const QByteArray &path, const QByteArray &query,
                                       const QByteArray &content, const QStringList &headers);

    // Public API. The public QtS3 class calls these.
    void preflight(const QByteArray &bucketName);
    QtS3ReplyPrivate *location(const QByteArray &bucketName);
    QtS3ReplyPrivate *put(const QByteArray &bucketName, const QString &path,
                          const QByteArray &content, const QStringList &headers);
    QtS3ReplyPrivate *exists(const QByteArray &bucketName, const QString &path);
    QtS3ReplyPrivate *size(const QByteArray &bucketName, const QString &path);
    QtS3ReplyPrivate *get(const QByteArray &bucketName, const QString &path);
    QtS3ReplyPrivate *remove(const QByteArray &bucket, const QString &path);

    void clearCaches();
    QByteArray accessKeyId();
    QByteArray secretAccessKey();
};

class QtS3ReplyPrivate
{
public:
    QtS3ReplyPrivate();
    QtS3ReplyPrivate(QtS3ReplyBase::S3Error, QString errorString);

    QByteArray m_byteArrayData;
    bool m_intAndBoolDataValid;
    int m_intAndBoolData;

    QNetworkReply *m_networkReply;

    QtS3ReplyBase::S3Error m_s3Error;
    QString m_s3ErrorString;

    bool isSuccess();
    QNetworkReply::NetworkError networkError();
    QString networkErrorString();
    QtS3ReplyBase::S3Error s3Error();
    QString s3ErrorString();
    QString anyErrorString();

    void prettyPrintReply();
    QByteArray headerValue(const QByteArray &headerName);

    bool boolValue();
    int intValue();
    QByteArray bytearrayValue();
};

QPM_END_NAMESPACE(com, github, msorvig, s3)

#endif
