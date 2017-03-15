#include <QtTest/QtTest>
#include <QtCore/QtCore>

#include "tst_qts3.h"
#include <qts3_p.h>

class TestQtS3 : public QObject
{
    Q_OBJECT
private slots:
    // helpers
    void dateTime();

    // signing key creation
    void deriveSigningKey();
    void checkGenerateSigningKey();

    // authorization header creation
    void formatQueryString();
    void formatCanonicalRequest();
    void formatStringToSign();
    void signStringToSign();
    void signRequestData();
    void formatAuthorizationHeader();
    void createAuthorizationHeader();

    // QNetworkRequest creation and signing
    void createAndSignRequest();

    // AWS test data
    void awsTestSuite_data();
    void awsTestSuite();

    // Integration tests that require netowork access
    // and access to a test bucket on S3.
    void location();
    void put();
    void exists();
    void size();
    void get();

    // Threaded integration tests
    void thread_putget();
};

// test date and time formatting
void TestQtS3::dateTime()
{
    QDate date(AwsTestData::timeStamp.date());
    QCOMPARE(QtS3Private::formatDate(date), AwsTestData::date);
    QDateTime dateTime(AwsTestData::timeStamp);
    QCOMPARE(QtS3Private::formatDateTime(dateTime), AwsTestData::dateTime);
}

// test S3 signing key derivation
void TestQtS3::deriveSigningKey()
{
    QByteArray signingKey = QtS3Private::deriveSigningKey(
        AwsTestData::secretAccessKey, AwsTestData::date, AwsTestData::region, AwsTestData::service);
    QCOMPARE(signingKey.toHex(), AwsTestData::signingKey);
}

// signing key (re-) generation. Verify that the key is generated only when needed
void TestQtS3::checkGenerateSigningKey()
{
    QByteArray key;
    QDateTime keyTime;
    QDateTime t0 = QDateTime(QDate(2000, 1, 1), QTime(0, 00));
    QDateTime t1 = QDateTime(QDate(2000, 1, 1), QTime(0, 30));    // + 30s
    QDateTime t2 = QDateTime(QDate(2000, 1, 2), QTime(0, 30));    // + 1 day: regenerates
    QDateTime t3 = QDateTime(QDate(9999, 12, 30), QTime(23, 59)); // + many years: regenerates
    QDateTime t4 = QDateTime(QDate(4000, 12, 30), QTime(23, 59)); // negative: regenerates

    QHash<QByteArray, QtS3Private::S3KeyStruct> signingKeys;

    QVERIFY(QtS3Private::checkGenerateSigningKey(&signingKeys, t0, AwsTestData::secretAccessKey,
                                                 AwsTestData::region, AwsTestData::service));
    QVERIFY(!QtS3Private::checkGenerateSigningKey(&signingKeys, t0, AwsTestData::secretAccessKey,
                                                  AwsTestData::region, AwsTestData::service));
    QVERIFY(!QtS3Private::checkGenerateSigningKey(&signingKeys, t1, AwsTestData::secretAccessKey,
                                                  AwsTestData::region, AwsTestData::service));
    QVERIFY(QtS3Private::checkGenerateSigningKey(&signingKeys, t2, AwsTestData::secretAccessKey,
                                                 AwsTestData::region, AwsTestData::service));
    QVERIFY(QtS3Private::checkGenerateSigningKey(&signingKeys, t3, AwsTestData::secretAccessKey,
                                                 AwsTestData::region, AwsTestData::service));
    QVERIFY(QtS3Private::checkGenerateSigningKey(&signingKeys, t4, AwsTestData::secretAccessKey,
                                                 AwsTestData::region, AwsTestData::service));
}

// test canonicalizing a query string.
void TestQtS3::formatQueryString()
{
    QByteArray canonicalQueryString =
        QtS3Private::createCanonicalQueryString(AwsTestData::inputQueryString);
    QCOMPARE(canonicalQueryString, AwsTestData::canonicalQueryString);
}

// test creating a canonical request + hash from request components
void TestQtS3::formatCanonicalRequest()
{
    // payload hashing
    QCOMPARE(QtS3Private::hash(AwsTestData::content).toHex(), AwsTestData::contentHash);

    // canonical request construction
    QByteArray canonicalRequest = QtS3Private::formatCanonicalRequest(
        AwsTestData::method, AwsTestData::url, AwsTestData::queryString, AwsTestData::headers,
        AwsTestData::contentHash);
    QCOMPARE(canonicalRequest, AwsTestData::canonicalRequest);

    // canonical request hashing
    QCOMPARE(QtS3Private::hash(canonicalRequest).toHex(), AwsTestData::canonicalRequestHash);
}

// test formatting the "string to sign"
void TestQtS3::formatStringToSign()
{
    QByteArray stringToSign =
        QtS3Private::formatStringToSign(AwsTestData::timeStamp, AwsTestData::region,
                                        AwsTestData::service, AwsTestData::canonicalRequestHash);
    QCOMPARE(stringToSign, AwsTestData::stringToSign);
}

// test signing the "string to sign"
void TestQtS3::signStringToSign()
{
    QByteArray signature =
        QtS3Private::sign(QByteArray::fromHex(AwsTestData::signingKey), AwsTestData::stringToSign);
    QCOMPARE(signature.toHex(), AwsTestData::signature);
}

// test signing the request data
void TestQtS3::signRequestData()
{
    QByteArray signature = QtS3Private::signRequestData(
        AwsTestData::headers, AwsTestData::method, AwsTestData::url, QByteArray(),
        AwsTestData::content, QByteArray::fromHex(AwsTestData::signingKey), AwsTestData::timeStamp,
        AwsTestData::region, AwsTestData::service);
    QCOMPARE(signature.toHex(), AwsTestData::signature);
}

// test formatting the authorization header
void TestQtS3::formatAuthorizationHeader()
{
    QByteArray authHeaderValue = QtS3Private::formatAuthorizationHeader(
        AwsTestData::accessKeyId, AwsTestData::timeStamp, AwsTestData::region, AwsTestData::service,
        AwsTestData::signedHeaders, AwsTestData::signature);
    QCOMPARE(authHeaderValue, AwsTestData::authorizationHeaderValue);
}

// test creating the authorization header
void TestQtS3::createAuthorizationHeader()
{
    QByteArray authHeaderValue = QtS3Private::createAuthorizationHeader(
        AwsTestData::headers, AwsTestData::method, AwsTestData::url, QByteArray(),
        AwsTestData::content, AwsTestData::accessKeyId,
        QByteArray::fromHex(AwsTestData::signingKey), AwsTestData::timeStamp, AwsTestData::region,
        AwsTestData::service);
    QCOMPARE(authHeaderValue, AwsTestData::authorizationHeaderValue);
}

// test creating an signing a QNetworkRequest with QtS3Private
void TestQtS3::createAndSignRequest()
{
    // create request
    QNetworkRequest request;
    QtS3Private::setRequestAttributes(&request, QUrl(AwsTestData::url), AwsTestData::headers,
                                      AwsTestData::timeStamp, AwsTestData::host);

    //
    // QCOMPARE(QtS3Private::requestHeaders(&request), AwsTestData::headers);

    // sign request
    QtS3Private::signRequest(&request, AwsTestData::method, AwsTestData::content,
                             AwsTestData::accessKeyId, QByteArray::fromHex(AwsTestData::signingKey),
                             AwsTestData::timeStamp, AwsTestData::region, AwsTestData::service);
    QByteArray authorizationHeaderName = "Authorization";
    QVERIFY(request.rawHeaderList().contains(authorizationHeaderName));

    // QCOMPARE(request.rawHeader(authorizationHeaderName), AwsTestData::authorizationHeaderValue);
}

// test using test data from the suite at
// http://docs.aws.amazon.com/general/latest/gr/signature-v4-test-suite.html
// expexts to find the data in $PWD/aws4_testsuite
void TestQtS3::awsTestSuite_data()
{
    QTest::addColumn<QString>("fileName");
    QStringList tests = QDir("./aws4_testsuite/").entryList(QStringList() << "*.req");
    foreach (const QString &test, tests) {
        QString baseName = test;
        baseName.chop(4); // remove ".req" at end
        QTest::newRow(baseName.toLatin1().constData()) << baseName;
    }
}

QByteArray readFile(const QString &fileName)
{
    QFile f(fileName);
    f.open(QIODevice::ReadOnly);
    QByteArray contents = f.readAll();

    contents.replace("\r", "");

    return contents;
}

void TestQtS3::awsTestSuite()
{
    QFETCH(QString, fileName);

    // skip multiline headers tests, test file format parser can't cope with it.
    if (fileName.contains("multiline"))
        QSKIP("");
    if (fileName.contains("slash") || fileName.contains("relative"))
        QSKIP("");
    if (fileName.contains("duplicate") || fileName.contains("value-order"))
        QSKIP("");
    if (fileName.contains("nonunreserved") || fileName.contains("urlencoded"))
        QSKIP("");

    // fileName is the base name. The following files exist:
    // .req : the input request
    // .crec: canonical request
    // .sts : string to sign
    // .athz: authorization header (value)
    // .sreq: signed request
    QString requestFile = "./aws4_testsuite/" + fileName + ".req";
    QString canonicalRequestFile = "./aws4_testsuite/" + fileName + ".creq";
    QString stringToSignFile = "./aws4_testsuite/" + fileName + ".sts";
    QString authorizationHeaderFile = "./aws4_testsuite/" + fileName + ".authz";
    QString signedRequestFile = "./aws4_testsuite/" + fileName + ".sreq";

    if (!QFile(requestFile).exists())
        QSKIP("AWS test suite not found in aws4_testsuite");

    // read and parse the request
    QByteArray request = readFile(requestFile);

    // first line VERB path query string
    QList<QByteArray> lines = request.split('\n');
    QList<QByteArray> line0Parts = lines[0].split(' ');
    QByteArray verb = line0Parts[0];

    QByteArray url = line0Parts[1];
    QByteArray path;
    QByteArray query;
    int q = url.indexOf('?');
    if (url.contains('?')) {
        path = url.mid(0, q);
        query = url.mid(q + 1);
    } else {
        path = url;
    }

    // next, headers
    QHash<QByteArray, QByteArray> headers;
    for (int i = 1; i < lines.count(); ++i) {
        const QByteArray line = lines.at(i);
        if (line.contains(':')) {
            int c = line.indexOf(":");
            QByteArray key = line.mid(0, c);
            QByteArray value = line.mid(c + 1);
            QByteArray currentValue = headers.value(key);
            if (!currentValue.isEmpty())
                currentValue.append(",");
            currentValue.append(value), headers.insert(key, currentValue);
        }
    }
    QByteArray payload; // empty.
    QByteArray payloadHash = QtS3Private::hash(payload).toHex();

    // create and compare canonical request
    QByteArray canonicalRequest =
        QtS3Private::formatCanonicalRequest(verb, path, query, headers, payloadHash);
    QCOMPARE(canonicalRequest, readFile(canonicalRequestFile));

    // create and compare string to sign
    QDateTime timestamp(QDate(2011, 9, 9), QTime(23, 36, 0)); // <- fixed date for all tests
    QByteArray region = "us-east-1";
    QByteArray service = "host";
    QByteArray canonicalRequestHash = QtS3Private::hash(canonicalRequest).toHex();
    QByteArray stringToSign =
        QtS3Private::formatStringToSign(timestamp, region, service, canonicalRequestHash);
    QCOMPARE(stringToSign, readFile(stringToSignFile));

    // create and compare authorization header
    QByteArray accessKeyId = "AKIDEXAMPLE";
    QByteArray secretAccessKey = "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";
    QByteArray signingKey = QtS3Private::deriveSigningKey(
        secretAccessKey, QtS3Private::formatDate(timestamp.date()), region, service);
    QByteArray authorizationHeader = QtS3Private::createAuthorizationHeader(
        headers, verb, path, query, payload, accessKeyId, signingKey, timestamp, region, service);
    QCOMPARE(authorizationHeader, readFile(authorizationHeaderFile));
}

void TestQtS3::location()
{
    // Get key id and secret key from environment
    QByteArray awsKeyId = qgetenv("QTS3_TEST_ACCESS_KEY_ID");
    QByteArray awsSecretKey = qgetenv("QTS3_TEST_SECRET_ACCESS_KEY");
    QByteArray testBucketUs = qgetenv("QTS3_TEST_BUCKET_US");
    QByteArray testBucketEu = qgetenv("QTS3_TEST_BUCKET_EU");

    if (awsKeyId.isEmpty())
        QSKIP("QTS3_TEST_ACCESS_KEY_ID not set. This tests requires S3 access.");
    if (awsSecretKey.isEmpty())
        QSKIP("QTS3_TEST_SECRET_ACCESS_KEY not set. This tests requires S3 access.");
    if (testBucketUs.isEmpty() || testBucketEu.isEmpty())
        QSKIP("QTS3_TEST_BUCKET_US or QTS3_TEST_BUCKET_EU not set. Should be set to a"
              "us-east-1 and eu-west-1 bucket with write access");

    QtS3 s3(awsKeyId, awsSecretKey);

    // Error case: empty bucket name
    {
        QtS3Reply<QByteArray> reply = s3.location("");
        QVERIFY(!reply.isSuccess());
        QCOMPARE(reply.s3Error(), QtS3ReplyBase::BucketNameInvalidError);
    }

    // Error case: bucket not found
    {
        QtS3Reply<QByteArray> reply =
            s3.location("sdfkljrsldkfjsdlfkajsdflasdjfldksjfkjdhgfkjghfdkjg");
        QVERIFY(!reply.isSuccess());
        QCOMPARE(reply.s3Error(), QtS3ReplyBase::BucketNotFoundError);
    }

    // US bucket
    {
        QtS3Reply<QByteArray> reply = s3.location("qtestbucket-us");
        QVERIFY(reply.isSuccess());
        QCOMPARE(reply.value(), QByteArray("us-east-1"));
    }

    // EU bucket
    {
        QtS3Reply<QByteArray> reply = s3.location(testBucketEu);
        QVERIFY(reply.isSuccess());
        QCOMPARE(reply.value(), QByteArray("eu-west-1"));
    }
}

void TestQtS3::put()
{
    // Get key id and secret key from environment
    QByteArray awsKeyId = qgetenv("QTS3_TEST_ACCESS_KEY_ID");
    QByteArray awsSecretKey = qgetenv("QTS3_TEST_SECRET_ACCESS_KEY");
    QByteArray testBucketUs = qgetenv("QTS3_TEST_BUCKET_US");
    QByteArray testBucketEu = qgetenv("QTS3_TEST_BUCKET_EU");

    if (awsKeyId.isEmpty())
        QSKIP("QTS3_TEST_ACCESS_KEY_ID not set. This tests requires S3 access.");
    if (awsSecretKey.isEmpty())
        QSKIP("QTS3_TEST_SECRET_ACCESS_KEY not set. This tests requires S3 access.");
    if (testBucketUs.isEmpty() || testBucketEu.isEmpty())
        QSKIP("QTS3_TEST_BUCKET_US or QTS3_TEST_BUCKET_EU not set. Should be set to a"
              "us-east-1 and eu-west-1 bucket with write access");

    QtS3 s3(awsKeyId, awsSecretKey);

    // Error case - bucket not found
    {
        QtS3Reply<void> reply = s3.put("skfjhagkljdfhgslkdjhgsdlkfjghsdfklgjhsdflkgjshdflgkjsdfhg",
                                       "foo-object", "foo-content", QStringList());
        QVERIFY(!reply.isSuccess());
        QCOMPARE(reply.s3Error(), QtS3ReplyBase::BucketNotFoundError);
    }

    // US bucket
    {
        QtS3Reply<void> reply =
            s3.put("qtestbucket-us", "foo-object", "foo-content-us", QStringList());
        QVERIFY(reply.isSuccess());
        QCOMPARE(reply.s3Error(), QtS3ReplyBase::NoError);
        QCOMPARE(reply.s3ErrorString(), QString());
    }

    // EU bucket
    {
        QtS3Reply<void> reply =
            s3.put(testBucketEu, "foo-object", "foo-content-eu", QStringList());
        QVERIFY(reply.isSuccess());
        QCOMPARE(reply.s3Error(), QtS3ReplyBase::NoError);
        QCOMPARE(reply.s3ErrorString(), QString());
    }
}

void TestQtS3::exists()
{
    QByteArray awsKeyId = qgetenv("QTS3_TEST_ACCESS_KEY_ID");
    QByteArray awsSecretKey = qgetenv("QTS3_TEST_SECRET_ACCESS_KEY");
    QByteArray testBucketUs = qgetenv("QTS3_TEST_BUCKET_US");
    QByteArray testBucketEu = qgetenv("QTS3_TEST_BUCKET_EU");

    if (awsKeyId.isEmpty())
        QSKIP("QTS3_TEST_ACCESS_KEY_ID not set. This tests requires S3 access.");
    if (awsSecretKey.isEmpty())
        QSKIP("QTS3_TEST_SECRET_ACCESS_KEY not set. This tests requires S3 access.");
    if (testBucketUs.isEmpty() || testBucketEu.isEmpty())
        QSKIP("QTS3_TEST_BUCKET_US or QTS3_TEST_BUCKET_EU not set. Should be set to a"
              "us-east-1 and eu-west-1 bucket with write access");


    QtS3 s3(awsKeyId, awsSecretKey);

    {
        QtS3Reply<bool> exists = s3.exists("qtestbucket-us", "foo-object");
        QVERIFY(exists.isSuccess());
        QVERIFY(exists.value());
    }

    {
        QtS3Reply<bool> exists = s3.exists("qtestbucket-us", "foo-object-notcreated");
        QVERIFY(exists.isSuccess());
        QVERIFY(!exists.value());
    }
}

void TestQtS3::size()
{
    QByteArray awsKeyId = qgetenv("QTS3_TEST_ACCESS_KEY_ID");
    QByteArray awsSecretKey = qgetenv("QTS3_TEST_SECRET_ACCESS_KEY");
    QByteArray testBucketUs = qgetenv("QTS3_TEST_BUCKET_US");
    QByteArray testBucketEu = qgetenv("QTS3_TEST_BUCKET_EU");

    if (awsKeyId.isEmpty())
        QSKIP("QTS3_TEST_ACCESS_KEY_ID not set. This tests requires S3 access.");
    if (awsSecretKey.isEmpty())
        QSKIP("QTS3_TEST_SECRET_ACCESS_KEY not set. This tests requires S3 access.");
    if (testBucketUs.isEmpty() || testBucketEu.isEmpty())
        QSKIP("QTS3_TEST_BUCKET_US or QTS3_TEST_BUCKET_EU not set. Should be set to a"
              "us-east-1 and eu-west-1 bucket with write access");


    QtS3 s3(awsKeyId, awsSecretKey);

    // exisiting files
    {
        QtS3Reply<int> sizeReply = s3.size("qtestbucket-us", "foo-object");
        QVERIFY(sizeReply.isSuccess());
        QCOMPARE(sizeReply.value(), 14);
    }

    // Error case: object does not exist
    {
        QtS3Reply<int> sizeReply = s3.size("qtestbucket-us", "foo-object-notcreated");
        QVERIFY(!sizeReply.isSuccess());
        // value is undefined
    }
}

void TestQtS3::get()
{
    QByteArray awsKeyId = qgetenv("QTS3_TEST_ACCESS_KEY_ID");
    QByteArray awsSecretKey = qgetenv("QTS3_TEST_SECRET_ACCESS_KEY");
    QByteArray testBucketUs = qgetenv("QTS3_TEST_BUCKET_US");
    QByteArray testBucketEu = qgetenv("QTS3_TEST_BUCKET_EU");

    if (awsKeyId.isEmpty())
        QSKIP("QTS3_TEST_ACCESS_KEY_ID not set. This tests requires S3 access.");
    if (awsSecretKey.isEmpty())
        QSKIP("QTS3_TEST_SECRET_ACCESS_KEY not set. This tests requires S3 access.");
    if (testBucketUs.isEmpty() || testBucketEu.isEmpty())
        QSKIP("QTS3_TEST_BUCKET_US or QTS3_TEST_BUCKET_EU not set. Should be set to a"
              "us-east-1 and eu-west-1 bucket with write access");


    QtS3 s3(awsKeyId, awsSecretKey);

    // Error case: Empty bucket name
    {
        QtS3Reply<QByteArray> contents = s3.get("", "");
        QCOMPARE(contents.s3Error(), QtS3ReplyBase::BucketNameInvalidError);
    }

    // Error case: Empty path
    {
        QtS3Reply<QByteArray> contents = s3.get("qtestbucket-us", "");
        QCOMPARE(contents.s3Error(), QtS3ReplyBase::ObjectNameInvalidError);
    }

    // Error case: Bucket not found
    {
        QtS3Reply<QByteArray> contents = s3.get("jkfghskjflahsfklajshdfkasjdhflskdj", "foo-object");
        QCOMPARE(contents.s3Error(), QtS3ReplyBase::BucketNotFoundError);
    }

    // Error case: Path not found
    {
        QtS3Reply<QByteArray> contents = s3.get("qtestbucket-us", "lskfjsloafkjfldkj");
        QCOMPARE(contents.s3Error(), QtS3ReplyBase::ObjectNotFoundError);
    }

    // Us bucket
    {
        QtS3Reply<QByteArray> contents = s3.get("qtestbucket-us", "foo-object");
        QVERIFY(contents.isSuccess());
        QCOMPARE(contents.value(), QByteArray("foo-content-us"));
    }

    // Eu bucket
    {
        QtS3Reply<QByteArray> contents = s3.get(testBucketEu, "foo-object");
        QVERIFY(contents.isSuccess());
        QCOMPARE(contents.value(), QByteArray("foo-content-eu"));
    }
}

template <typename F> class Runnable : public QRunnable
{
public:
    Runnable(const F &f, int threadCount, QThreadPool *threadPool)
        : m_f(f), m_threadCount(threadCount), m_threadPool(threadPool)
    {
        setAutoDelete(false);
    }

    void run()
    {
        int threadCountCopy = m_threadCount;
        m_threadCount = 0;
        for (int i = 0; i < threadCountCopy; ++i)
            m_threadPool->tryStart(this);
        m_f();
    }

    F m_f;
    int m_threadCount;
    QThreadPool *m_threadPool;
};

template <typename F> void runOnThreads(int threadCount, F lambda)
{
    QThreadPool pool;
    pool.setMaxThreadCount(threadCount);

    Runnable<F> runnable(lambda, threadCount - 1, &pool);
    pool.start(&runnable);

    pool.waitForDone();
}

void TestQtS3::thread_putget()
{
    QByteArray testBucketUs = qgetenv("QTS3_TEST_BUCKET_US");
    QByteArray testBucketEu = qgetenv("QTS3_TEST_BUCKET_EU");

    if (testBucketUs.isEmpty() || testBucketEu.isEmpty())
        QSKIP("QTS3_TEST_BUCKET_US or QTS3_TEST_BUCKET_EU not set. Should be set to a"
              "us-east-1 and eu-west-1 bucket with write access");

    QtS3 s3([]() { return qgetenv("QTS3_TEST_ACCESS_KEY_ID"); },
            []() { return qgetenv("QTS3_TEST_SECRET_ACCESS_KEY"); });

    if (s3.accessKeyId().isEmpty())
        QSKIP("QTS3_TEST_ACCESS_KEY_ID not set. This tests requires S3 access.");
    if (s3.secretAccessKey().isEmpty())
        QSKIP("QTS3_TEST_SECRET_ACCESS_KEY not set. This tests requires S3 access.");


    runOnThreads(50, [&s3, testBucketUs, testBucketEu]()
    {
        {
            QtS3Reply<void> reply =
                s3.put(testBucketEu, "foo-object", "foo-content-eu", QStringList());
            QVERIFY(reply.isSuccess());
        }

        {
            QtS3Reply<QByteArray> contents = s3.get(testBucketEu, "foo-object");
            QVERIFY(contents.isSuccess());
            QCOMPARE(contents.value(), QByteArray("foo-content-eu"));
        }
    });
}

QTEST_MAIN(TestQtS3)

#include "tst_qts3.moc"
