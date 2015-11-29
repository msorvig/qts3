#include <QtCore>

#include "qts3_p.h"

QPM_BEGIN_NAMESPACE(com, github, msorvig, s3)

// Hello! This file implements:
//
//   - Generic AWS request signing (any service, using AWS4-HMAC-SHA256)
//   - QNetworkAccessManager AWS request handling.
//   - The public QtS3 API
//
//  Data flow for requests:
//
//                        AWS Secret key ->
//                                Date   -> Signing
//                               Region  ->   Key    ---------------|
//                               Service ->                         |
//                                                                  |
//        Headers: Host, X-Amz-Date                                 |  DateTime
//  App                 |                                           |  Date/region/service
//   |                  |                                           |    |
//    Request  -> Canonical Request -> CanonicalRequest Hash -> String to Sign -> Request Signature
//        |                                                             |             |
//    Request                                                           |             |
//   with Auth  <---------------Athentication Header-----------------------------------
//     header                             ^
//        |                          AWS Key Id
//    THE INTERNET
//
//
//  Control flow for request signing:
//
//  checkGenerateSigningKey
//      deriveSigningKey
//
//  createAuthorizationHeader
//      signRequestData
//          formatCanonicalRequest
//          formatStringToSign
//      formatAuthorizationHeader
//

Q_LOGGING_CATEGORY(qts3, "qts3.API")
Q_LOGGING_CATEGORY(qts3_Internal, "qts3.internal")

QtS3ReplyPrivate::QtS3ReplyPrivate()
    : m_intAndBoolDataValid(false), m_networkReply(0),
      m_s3Error(QtS3ReplyBase::InternalReplyInitializationError),
      m_s3ErrorString("Internal error: un-initianlized QtS3Reply.")
{

}

QtS3ReplyPrivate::QtS3ReplyPrivate(QtS3ReplyBase::S3Error error, QString errorString)
    : m_intAndBoolDataValid(false), m_networkReply(0), m_s3Error(error),
      m_s3ErrorString(errorString)
{

}

QNetworkReply::NetworkError QtS3ReplyPrivate::networkError()
{
    return m_networkReply ? m_networkReply->error() : QNetworkReply::NoError;
}

QString QtS3ReplyPrivate::networkErrorString()
{
    return m_networkReply ? m_networkReply->errorString() : QString();
}

QtS3ReplyBase::S3Error QtS3ReplyPrivate::s3Error() { return m_s3Error; }

QString QtS3ReplyPrivate::s3ErrorString() { return m_s3ErrorString; }

QString QtS3ReplyPrivate::anyErrorString()
{
    if (networkError() != QNetworkReply::NoError)
        return networkErrorString();
    if (s3Error() != QtS3ReplyBase::NoError)
        return s3ErrorString();
    return QString();
}

void QtS3ReplyPrivate::prettyPrintReply()
{
    qDebug() << "Reply:                   :" << this;
    qDebug() << "Reply Error State        :" << s3Error() << s3ErrorString();
    if (m_networkReply == 0) {
        qDebug() << "m_networkReply is null";
        return;
    }

    qDebug() << "NetworkReply Error State :" << m_networkReply->error()
             << m_networkReply->errorString();
    qDebug() << "NetworkReply Headers:";
    foreach (auto pair, m_networkReply->rawHeaderPairs()) {
        qDebug() << "   " << pair.first << pair.second;
    }
    qDebug() << "S3 reply data            :" << bytearrayValue();
}

QByteArray QtS3ReplyPrivate::headerValue(const QByteArray &headerName)
{
    if (!m_networkReply)
        return QByteArray();
    return m_networkReply->rawHeader(headerName);
}

bool QtS3ReplyPrivate::isSuccess() { return m_s3Error == QtS3ReplyBase::NoError; }

bool QtS3ReplyPrivate::boolValue()
{
    if (m_intAndBoolDataValid)
        return bool(m_intAndBoolData);
    return false;
}

int QtS3ReplyPrivate::intValue()
{
    if (m_intAndBoolDataValid)
        return m_intAndBoolData;
    return 0;
}

QByteArray QtS3ReplyPrivate::bytearrayValue() { return m_byteArrayData; }

QtS3Private::QtS3Private() : m_networkAccessManager(0) {}

QtS3Private::QtS3Private(QByteArray accessKeyId, QByteArray secretAccessKey)
{
    m_accessKeyIdProvider = [accessKeyId](){ return accessKeyId; };
    m_secretAccessKeyProvider = [secretAccessKey](){ return secretAccessKey; };

    init();
}

QtS3Private::QtS3Private(std::function<QByteArray()> accessKeyIdProvider,
                         std::function<QByteArray()> secretAccessKeyProvider)
    : m_accessKeyIdProvider(accessKeyIdProvider),
      m_secretAccessKeyProvider(secretAccessKeyProvider)
{
    init();
}

QtS3Private::~QtS3Private()
{
    if (m_networkAccessManager->pendingRequests() > 0)
        qWarning() << "QtS3 object deleted with pending requests in flight";
}

// Returns a date formatted as YYYYMMDD.
QByteArray QtS3Private::formatDate(const QDate &date)
{
    return date.toString(QStringLiteral("yyyyMMdd")).toLatin1();
}

// Returns a date formatted as YYYYMMDDTHHMMSSZ
QByteArray QtS3Private::formatDateTime(const QDateTime &dateTime)
{
    return dateTime.toString(QStringLiteral("yyyyMMddThhmmssZ")).toLatin1();
}

// SHA256.
QByteArray QtS3Private::hash(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

// HMAC_SHA256.
QByteArray QtS3Private::sign(const QByteArray &key, const QByteArray &data)
{
    return QMessageAuthenticationCode::hash(data, key, QCryptographicHash::Sha256);
}

// Canonicalizes a list of http headers.
//    * sorted on header key (using QMap)
//    * whitespace trimmed.
QMap<QByteArray, QByteArray> QtS3Private::canonicalHeaders(
    const QHash<QByteArray, QByteArray> &headers)
{
    QMap<QByteArray, QByteArray> canonical;

    for (auto it = headers.begin(); it != headers.end(); ++it)
        canonical[it.key().toLower()] = it.value().trimmed();
    return canonical;
}

// Creates newline-separated list of headers formatted like "name:values"
QByteArray QtS3Private::formatHeaderNameValueList(const QMap<QByteArray, QByteArray> &headers)
{
    QByteArray nameValues;
    for (auto it = headers.begin(); it != headers.end(); ++it)
        nameValues += it.key() + ":" + it.value() + "\n";
    return nameValues;
}

// Creates a semicolon-separated list of header names
QByteArray QtS3Private::formatHeaderNameList(const QMap<QByteArray, QByteArray> &headers)
{
    QByteArray names;
    for (auto it = headers.begin(); it != headers.end(); ++it)
        names += it.key() + ";";
    names.chop(1); // remove final ";"
    return names;
}

// Creates a canonical query string by sorting and percent encoding the query
QByteArray QtS3Private::createCanonicalQueryString(const QByteArray &queryString)
{
    // General querystring form
    // "Foo1=bar1&"
    // "Foo2=bar2&"

    // remove any leading '?'
    QByteArray input = queryString;
    //    if (input.startsWith("?"))
    //        input.remove(0, 1);

    // split and sort querty parts
    QList<QByteArray> parts = input.split('&');
    qSort(parts);
    // write out percent encoded canonical string.
    QByteArray canonicalQuery;
    canonicalQuery.reserve(queryString.size());
    //    canonicalQuery.append("%3F");
    foreach (const QByteArray &part, parts) {
        QByteArray encodedPart = part.toPercentEncoding("=%"); // skip = and % for aws complicance
        if (!encodedPart.isEmpty() && !encodedPart.contains('='))
            encodedPart.append("=");

        canonicalQuery.append(encodedPart);
        canonicalQuery.append('&');
    }
    canonicalQuery.chop(1); // remove final '&'

    return canonicalQuery;
}

//  Derives an AWS version 4 signing key. \a secretAccessKey is the aws secrect key,
//  \a dateString is a YYYYMMDD date. The signing key is valid for a limited number of days
//  (currently 7). \a region is the bucket region, for example "us-east-1". \a service the
//  aws service ("s3", ...)
QByteArray QtS3Private::deriveSigningKey(const QByteArray &secretAccessKey,
                                         const QByteArray dateString, const QByteArray &region,
                                         const QByteArray &service)
{
    return sign(sign(sign(sign("AWS4" + secretAccessKey, dateString), region), service),
                "aws4_request");
}

// Generates a new AWS signing key when required. This will typically happen
// on the first call or when the key expires. QtS3 expires the key after one
// day, well before the (current) AWS 7-day expiry period. The key is tied
// to the bucket region and the s3 service. Returns whether the a key was
// created.
bool QtS3Private::checkGenerateSigningKey(QHash<QByteArray, QtS3Private::S3KeyStruct> *signingKeys,
                                          const QDateTime &now, const QByteArray &secretAccessKey,
                                          const QByteArray &region, const QByteArray &service)
{
    const int secondsInDay = 60 * 60 * 24;

    auto it = signingKeys->find(region);
    if (it != signingKeys->end() && it->timeStamp.isValid()) {
        const qint64 keyAge = it->timeStamp.secsTo(now);
        if (keyAge >= 0 && keyAge < secondsInDay)
            return false;
    }

    QByteArray key = deriveSigningKey(secretAccessKey, formatDate(now.date()), region, service);
    S3KeyStruct keyStruct = {now, key};
    signingKeys->insert(region, keyStruct);

    return true;
}

// Creates a "stringToSign" (see aws documentation).
QByteArray QtS3Private::formatStringToSign(const QDateTime &timeStamp, const QByteArray &region,
                                           const QByteArray &service,
                                           const QByteArray &canonicalRequestHash)
{
    QByteArray string = "AWS4-HMAC-SHA256\n" + formatDateTime(timeStamp) + "\n"
                        + formatDate(timeStamp.date()) + "/" + region + "/" + service
                        + "/aws4_request\n" + canonicalRequestHash;
    return string;
}

// Formats an authorization header.
QByteArray QtS3Private::formatAuthorizationHeader(
    const QByteArray &awsAccessKeyId, const QDateTime &timeStamp, const QByteArray &region,
    const QByteArray &service, const QByteArray &signedHeaders, const QByteArray &signature)
{
    QByteArray headerValue = "AWS4-HMAC-SHA256 "
                             "Credential=" + awsAccessKeyId + "/" + formatDate(timeStamp.date())
                             + "/" + region + "/" + service + "/aws4_request, " + +"SignedHeaders="
                             + signedHeaders + ", " + "Signature=" + signature;
    return headerValue;
}

// Copies the request headers from a QNetworkRequest.
QHash<QByteArray, QByteArray> QtS3Private::requestHeaders(const QNetworkRequest *request)
{
    QHash<QByteArray, QByteArray> headers;
    for (const QByteArray &header : request->rawHeaderList()) {
        headers.insert(header, request->rawHeader(header));
    }
    return headers;
}

QHash<QByteArray, QByteArray> QtS3Private::parseHeaderList(const QStringList &headers)
{
    QHash<QByteArray, QByteArray> parsedHeaders;
    foreach (const QString &header, headers) {
        QStringList parts = header.split(":");
        parsedHeaders.insert(parts.at(0).toLatin1(), parts.at(1).toLatin1());
    }
    return parsedHeaders;
}

// Creates a canonical request string (example):
//     POST
//     /
//
//     content-type:application/x-www-form-urlencoded; charset=utf-8\n
//     host:iam.amazonaws.com\n
//     x-amz-date:20110909T233600Z\n
//
//     content-type;host;x-amz-date\n
//     b6359072c78d70ebee1e81adcbab4f01bf2c23245fa365ef83fe8f1f955085e2
QByteArray QtS3Private::formatCanonicalRequest(const QByteArray &method, const QByteArray &url,
                                               const QByteArray &queryString,
                                               const QHash<QByteArray, QByteArray> &headers,
                                               const QByteArray &payloadHash)
{
    const auto canonHeaders = canonicalHeaders(headers);
    const int estimatedLength = method.length() + url.length() + queryString.length()
                                + canonHeaders.size() * 10 + payloadHash.length();

    QByteArray request;
    request.reserve(estimatedLength);
    request += method;
    request += "\n";
    request += url;
    request += "\n";
    request += createCanonicalQueryString(queryString);
    request += "\n";
    request += formatHeaderNameValueList(canonHeaders);
    request += "\n";
    request += formatHeaderNameList(canonHeaders);
    request += "\n";
    request += payloadHash;
    return request;
}

QByteArray QtS3Private::signRequestData(const QHash<QByteArray, QByteArray> headers,
                                        const QByteArray &verb, const QByteArray &url,
                                        const QByteArray &queryString, const QByteArray &payload,
                                        const QByteArray &signingKey, const QDateTime &dateTime,
                                        const QByteArray &region, const QByteArray &service)
{
    // create canonical request representation and hash
    QByteArray payloadHash = hash(payload).toHex();
    QByteArray canonicalRequest =
        formatCanonicalRequest(verb, url, queryString, headers, payloadHash);
    QByteArray canonialRequestHash = hash(canonicalRequest).toHex();

    // create (and sign) stringToSign
    QByteArray stringToSign = formatStringToSign(dateTime, region, service, canonialRequestHash);
    QByteArray signature = sign(signingKey, stringToSign);

    return signature;
}

QByteArray QtS3Private::createAuthorizationHeader(
    const QHash<QByteArray, QByteArray> headers, const QByteArray &verb, const QByteArray &url,
    const QByteArray &queryString, const QByteArray &payload, const QByteArray &accessKeyId,
    const QByteArray &signingKey, const QDateTime &dateTime, const QByteArray &region,
    const QByteArray &service)
{
    // sign request
    QByteArray signature = signRequestData(headers, verb, url, queryString, payload, signingKey,
                                           dateTime, region, service);

    // crate Authorization header;
    QByteArray headerNames = formatHeaderNameList(canonicalHeaders(headers));
    return formatAuthorizationHeader(accessKeyId, dateTime, region, service, headerNames,
                                     signature.toHex());
}

void QtS3Private::setRequestAttributes(QNetworkRequest *request, const QUrl &url,
                                       const QHash<QByteArray, QByteArray> &headers,
                                       const QDateTime &timeStamp, const QByteArray &host)
{
    // Build request from user input
    request->setUrl(url);
    for (auto it = headers.begin(); it != headers.end(); ++it)
        request->setRawHeader(it.key(), it.value());

    // Add standard AWS headers
    request->setRawHeader("User-Agent", "Qt");
    request->setRawHeader("Host", host);
    request->setRawHeader("X-Amz-Date", formatDateTime(timeStamp));
}

// Signs an aws request by adding an authorization header
void QtS3Private::signRequest(QNetworkRequest *request, const QByteArray &verb,
                              const QByteArray &payload, const QByteArray accessKeyId,
                              const QByteArray &signingKey, const QDateTime &dateTime,
                              const QByteArray &region, const QByteArray &service)
{

    QByteArray payloadHash = hash(payload).toHex();
    request->setRawHeader("x-amz-content-sha256", payloadHash);

    // get headers from request
    QHash<QByteArray, QByteArray> headers = requestHeaders(request);
    QUrl url = request->url();
    // create authorization header (value)
    QByteArray authHeaderValue
        = createAuthorizationHeader(headers, verb, url.path().toLatin1(), url.query().toLatin1(),
                                    payload, accessKeyId, signingKey, dateTime, region, service);
    // add authorization header to request
    request->setRawHeader("Authorization", authHeaderValue);
}

//
// Stateful non-static functons below
//

void QtS3Private::init()
{
    if (m_accessKeyIdProvider().isEmpty()) {
        qWarning() << "access key id not specified";
    }

    if (m_secretAccessKeyProvider().isEmpty()) {
        qWarning() << "secret access key not set";
    }

    m_service = "s3";

    // The current design multiplexes requests from several QtS3 request threads
    // to one QNetworkAccessManager on a network thread. This limits the number
    // of concurrent network requests to the QNetworkAccessManager internal limit
    // (rumored to be 6).
    m_networkAccessManager = new ThreadsafeBlockingNetworkAccesManager();
}

void QtS3Private::checkGenerateS3SigningKey(const QByteArray &region)
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    m_signingKeysLock.lockForWrite();
    checkGenerateSigningKey(&m_signingKeys, now, m_secretAccessKeyProvider(), region, m_service);
    m_signingKeysLock.unlock();
    // std::tuple keyTimeCopy { currents3SigningKey, s3SigningKeyTimeStamp };
    // return keyTimeCopy;
}

QNetworkRequest *QtS3Private::createSignedRequest(const QByteArray &verb, const QUrl &url,
                                                  const QHash<QByteArray, QByteArray> &headers,
                                                  const QByteArray &host, const QByteArray &payload,
                                                  const QByteArray &region)
{
    checkGenerateS3SigningKey(region);
    QDateTime requestTime = QDateTime::currentDateTimeUtc();

    // Create and sign request
    QNetworkRequest *request = new QNetworkRequest();

    // request->setAttribute(QNetworkRequest::SynchronousRequestAttribute, true);
    // request->setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

    setRequestAttributes(request, url, headers, requestTime, host);
    m_signingKeysLock.lockForRead();
    QByteArray key = m_signingKeys.value(region).key;
    m_signingKeysLock.unlock();
    signRequest(request, verb, payload, m_accessKeyIdProvider(), key, requestTime, region, m_service);
    return request;
}

namespace
{
// The read buffer for the current in-flight request. The synchronous API guarantees
// that there will be only one reuest in progress per thread at any time.
__thread QBuffer *m_inFlightBuffer = 0;
}

QNetworkReply *QtS3Private::sendRequest(const QByteArray &verb, const QNetworkRequest &request,
                                        const QByteArray &payload)
{

    m_inFlightBuffer = 0;
    if (!payload.isEmpty()) {
        m_inFlightBuffer = new QBuffer(const_cast<QByteArray *>(&payload));
        m_inFlightBuffer->open(QIODevice::ReadOnly);
    }

    // Send request
    // QNetworkAccessManager *qnam = new QNetworkAccessManager;
    QNetworkReply *reply =
        m_networkAccessManager->sendCustomRequest(request, verb, m_inFlightBuffer);
    return reply;
}

QNetworkReply *QtS3Private::sendS3Request(const QByteArray &bucketName, const QByteArray &verb,
                                          const QString &path, const QByteArray &queryString,
                                          const QByteArray &content, const QStringList &headers)
{
    const QByteArray host = bucketName + ".s3.amazonaws.com";
    const QByteArray url = "https://" + host + "/" + path.toLatin1() + "?" + queryString;

    QHash<QByteArray, QByteArray> hashHeaders = parseHeaderList(headers);
    m_bucketRegionsLock.lockForRead();
    QByteArray region = m_bucketRegions.value(bucketName);
    m_bucketRegionsLock.unlock();
    if (region.isEmpty()) {
        // internal error
        qDebug() << "No region for" << bucketName;
    }

    QNetworkRequest *request =
        createSignedRequest(verb, QUrl(url), hashHeaders, host, content, region);
    return sendRequest(verb, *request, content);
}

QHash<QByteArray, QByteArray> QtS3Private::getErrorComponents(const QByteArray &errorString)
{
    QHash<QByteArray, QByteArray> hash;
    QByteArray currentElement;

    QXmlStreamReader xml(errorString);
    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement()) {
            currentElement = xml.name().toUtf8();
            hash[currentElement] = "";
        }
        if (xml.isCharacters()) {
            hash[currentElement] = xml.text().toUtf8();
        }
    }

    if (xml.hasError()) {
        qDebug() << "xml err";
    }

    return hash;
}

QByteArray QtS3Private::getStringToSign(const QByteArray &errorString)
{
    return getErrorComponents(errorString)["StringToSign"];
}

QByteArray QtS3Private::getCanonicalRequest(const QByteArray &errorString)
{
    return getErrorComponents(errorString)["CanonicalRequest"];
}

void QtS3Private::preflight(const QByteArray &bucketName)
{
    cacheBucketLocation(0, bucketName);
}

bool QtS3Private::checkBucketName(QtS3ReplyPrivate *s3Reply, const QByteArray &bucketName)
{
    if (bucketName.isEmpty()) {
        s3Reply->m_s3Error = QtS3ReplyBase::BucketNameInvalidError;
        s3Reply->m_s3ErrorString = QStringLiteral("Bucket name is empty");
        return false;
    }

    // http://docs.aws.amazon.com/AmazonS3/latest/dev/BucketRestrictions.html

    return true;
}

bool QtS3Private::checkPath(QtS3ReplyPrivate *s3Reply, const QByteArray &path)
{
    if (path.isEmpty()) {
        s3Reply->m_s3Error = QtS3ReplyBase::ObjectNameInvalidError;
        s3Reply->m_s3ErrorString = QStringLiteral("Object name is empty");
        return false;
    }

    // "generally safe key charackter set"
    //  Alphanumeric characters [0-9a-zA-Z]
    //  Special characters !, -, _, ., *, ', (, and )

    return true;
}

// The AWS signing key and generated StringToSign depends on the
// bucket region. This function maintains a map of bucketName ->
// region for all seen buckets. Returns whether the bucket location
// could be established.
bool QtS3Private::cacheBucketLocation(QtS3ReplyPrivate *s3Reply, const QByteArray &bucketName)
{
    // Check if bucket region is cached.
    m_bucketRegionsLock.lockForRead();
    if (m_bucketRegions.contains(bucketName)) {
        m_bucketRegionsLock.unlock();
        return true;
    }
    m_bucketRegionsLock.unlock();

    // Send location request.
    QtS3ReplyPrivate *locationReply = this->location_impl(bucketName);
    if (!locationReply->isSuccess()) {
        // propagate error, copy locationReply to s3Reply;
        if (s3Reply) {
            s3Reply->m_networkReply = locationReply->m_networkReply;
            s3Reply->m_s3Error = locationReply->m_s3Error;
            s3Reply->m_s3ErrorString = locationReply->m_s3ErrorString;
        }
        delete locationReply;
        return false;
    }
    QByteArray contents = locationReply->bytearrayValue();
    delete locationReply;

    // Check (again) if bucket region is cached.
    m_bucketRegionsLock.lockForRead();
    if (m_bucketRegions.contains(bucketName)) {
        m_bucketRegionsLock.unlock();
        return true;
    }
    m_bucketRegionsLock.unlock();

    // Update cache with bucket locaton
    m_bucketRegionsLock.lockForWrite();
    if (!m_bucketRegions.contains(bucketName))
        m_bucketRegions.insert(bucketName, contents);
    m_bucketRegionsLock.unlock();

    return true;
}

void QtS3Private::processNetworkReplyState(QtS3ReplyPrivate *s3Reply, QNetworkReply *networkReply)
{
    s3Reply->m_networkReply = networkReply;

    // No error
    if (networkReply->error() == QNetworkReply::NoError) {
        s3Reply->m_s3Error = QtS3ReplyBase::NoError;
        s3Reply->m_s3ErrorString = QString();
        return;
    }

    // By default, and if the error XML processing below fails,
    // set the S3 error to NetworkError and forward the error string.
    s3Reply->m_s3Error = QtS3ReplyBase::NetworkError;
    s3Reply->m_s3ErrorString = networkReply->errorString();

    // Read the reply content, which will typically contain an XML structure
    // describing the error
    s3Reply->m_byteArrayData = networkReply->readAll();
    if (s3Reply->m_byteArrayData.isEmpty())
        return;

    // errors: http://docs.aws.amazon.com/AmazonS3/latest/API/ErrorResponses.html
    QHash<QByteArray, QByteArray> components = getErrorComponents(s3Reply->m_byteArrayData);
    s3Reply->m_s3ErrorString.clear();
    if (components.contains("Error")) {
        QByteArray code = components.value("Code");
        if (code == "NoSuchBucket") {
            s3Reply->m_s3Error = QtS3ReplyBase::BucketNotFoundError;
        } else if (code == "NoSuchKey") {
            s3Reply->m_s3Error = QtS3ReplyBase::ObjectNotFoundError;
        } else {
            s3Reply->m_s3Error = QtS3ReplyBase::GenereicS3Error;
            s3Reply->m_s3ErrorString = code + ": ";
        }
        s3Reply->m_s3ErrorString.append(components.value("Message"));
    }
}

// Signing an S3 requrest requires knowing the bucket region. This function
// gets the bucket region by making a location request to us-east-1.
QtS3ReplyPrivate *QtS3Private::location_impl(const QByteArray &bucketName)
{
    QtS3ReplyPrivate *s3Reply = new QtS3ReplyPrivate;

    if (!checkBucketName(s3Reply, bucketName))
        return s3Reply;

    // Special url for discovering the bucket region:
    // https://s3.amazonaws.com/bucket-name?location
    const QByteArray host = "s3.amazonaws.com";
    const QByteArray url = "https://" + host + "/" + bucketName + "?location";
    QNetworkRequest *request = createSignedRequest(
        "GET", QUrl(url), QHash<QByteArray, QByteArray>(), host, QByteArray(), "us-east-1");
    QNetworkReply *networkReply = sendRequest("GET", *request, QByteArray());

    processNetworkReplyState(s3Reply, networkReply);

    // Extract the location fromt he response xml on success.
    if (s3Reply->m_s3Error == QtS3ReplyBase::NoError) {
        s3Reply->m_byteArrayData = networkReply->readAll();

        QHash<QByteArray, QByteArray> components = getErrorComponents(s3Reply->bytearrayValue());
        QByteArray location = components.value("LocationConstraint");
        // handle special case where the S3 API returns no location for the standard US locaton
        if (location.isEmpty())
            location = "us-east-1";

        s3Reply->m_byteArrayData = location;
    }

    return s3Reply;
}

QtS3ReplyPrivate *QtS3Private::processS3Request(const QByteArray &verb,
                                                const QByteArray &bucketName,
                                                const QByteArray &path, const QByteArray &query,
                                                const QByteArray &content,
                                                const QStringList &headers)
{
    QtS3ReplyPrivate *s3Reply = new QtS3ReplyPrivate;

    if (!checkBucketName(s3Reply, bucketName))
        return s3Reply;
    if (!checkPath(s3Reply, path))
        return s3Reply;
    if (!cacheBucketLocation(s3Reply, bucketName))
        return s3Reply;

    QNetworkReply *networkReply = sendS3Request(bucketName, verb, path, query, content, headers);

    processNetworkReplyState(s3Reply, networkReply);

    return s3Reply;
}

QtS3ReplyPrivate *QtS3Private::location(const QByteArray &bucketName)
{
    // qCDebug(qts3) << "location" << bucketName;
    return location_impl(bucketName);
}

QtS3ReplyPrivate *QtS3Private::put(const QByteArray &bucketName, const QString &path,
                                   const QByteArray &content, const QStringList &headers)
{
    // qCDebug(qts3) << "put" << bucketName << path << content.count();

    return processS3Request("PUT", bucketName, path.toUtf8(), QByteArray(), content, headers);
}

QtS3ReplyPrivate *QtS3Private::exists(const QByteArray &bucketName, const QString &path)
{
    // qCDebug(qts3) << "exists" << bucketName << path;

    QtS3ReplyPrivate *s3Reply = processS3Request("HEAD", bucketName, path.toUtf8(), QByteArray(),
                                                 QByteArray(), QStringList());

    // The HEAD request above does not close properly - AWS closes it on
    // timeout and we get a RemoteHostClosedError. If we get any headers
    // then consider the request a success and proceed.
    if (s3Reply->headerValue("x-amz-request-id").isEmpty()) {
        return s3Reply;
    }

    s3Reply->m_s3Error = QtS3ReplyBase::NoError;
    s3Reply->m_s3ErrorString.clear();

    // Use the presence of "Content-Length" to detect existence.
    bool ok;
    s3Reply->headerValue("Content-Length").toInt(&ok);
    s3Reply->m_intAndBoolData = ok;
    s3Reply->m_intAndBoolDataValid = true;

    return s3Reply;
}

QtS3ReplyPrivate *QtS3Private::size(const QByteArray &bucketName, const QString &path)
{
    // qCDebug(qts3) << "size" << bucketName << path;

    QtS3ReplyPrivate *s3Reply = processS3Request("HEAD", bucketName, path.toUtf8(), QByteArray(),
                                                 QByteArray(), QStringList());
    // The HEAD request above does not close properly - AWS closes it on
    // timeout and we get a RemoteHostClosedError. If we get any headers
    // then consider the request a success and proceed.
    if (s3Reply->headerValue("x-amz-request-id").isEmpty()) {
        return s3Reply;
    }

    // Use the presence of "Content-Length" to detect existence.
    if (s3Reply->headerValue("Content-Length").isEmpty()) {
        s3Reply->m_s3Error = QtS3ReplyBase::ObjectNotFoundError;
        s3Reply->m_s3ErrorString = QStringLiteral("Object Not Found");
        return s3Reply;
    }

    s3Reply->m_s3Error = QtS3ReplyBase::NoError;
    s3Reply->m_s3ErrorString.clear();

    bool ok;
    int size = s3Reply->headerValue("Content-Length").toInt(&ok);
    s3Reply->m_intAndBoolData = size;
    s3Reply->m_intAndBoolDataValid = true;

    return s3Reply;
}

QtS3ReplyPrivate *QtS3Private::get(const QByteArray &bucketName, const QString &path)
{
    // qCDebug(qts3) << "get" << bucketName << path;

    QtS3ReplyPrivate *s3Reply = processS3Request("GET", bucketName, path.toUtf8(), QByteArray(),
                                                 QByteArray(), QStringList());

    // Read content
    if (s3Reply->m_s3Error == QtS3ReplyBase::NoError) {
        s3Reply->m_byteArrayData = s3Reply->m_networkReply->readAll();
    }
    return s3Reply;
}

void QtS3Private::clearCaches()
{
    m_signingKeysLock.lockForWrite();
    m_signingKeys.clear();
    m_signingKeysLock.unlock();

    m_bucketRegionsLock.lockForWrite();
    m_bucketRegions.clear();
    m_bucketRegionsLock.unlock();
}

QByteArray QtS3Private::accessKeyId()
{
    return m_accessKeyIdProvider();
}

QByteArray QtS3Private::secretAccessKey()
{
    return m_secretAccessKeyProvider();
}

QPM_END_NAMESPACE(com, github, msorvig, s3)
