Qt C++ API for Amazon S3

This project provides a simple, synchronous, C++ API for amazon S3.

###Building

Include qts3.pri in your project, or com_github_msorvig_s3.pri for the
namespaced qpm version.

###Using

To begin, you need an AWS key, which consists of a key id and a secret:

    QByteArray awsKeyId = ...;
    QByteArray awsSecretKey = ...;

Construct a s3 object using the key:

    QtS3 s3(awsKeyId, awsSecretKey);

You can then upload and download files using put() and get()

    QByteArray contents = ...;
    QtS3Reply<void> reply = s3.put("mybucket", contents);

    QtS3Reply<QByteArray> reply = s3.get("mybucket");
    QByteArray contents = reply.value();

The s3 calls are synchronous and blocks until the response is available.
The bucket must exist and be accessible -- bucket management is not
covered by this API.

###Error Handling

There are two levels of errors: Network errors and S3 errors. Network
errors are QNetworkAccessManager errors, such as "no network connection".
S3 errors are of the type "bucket not found", "object name invalid".

All S3 accessor functions return a QtS3Reply object, which is templated
on the return type: 

    QtS3Reply<QByteArray> QtS3::get();
    QtS3Reply<int> QtS3::size()
    QtS3Reply<bool> QtS3::exists();

Error state checking;

    bool QtS3Reply<T>::isSuccess()
    QtS3Reply<T>::networkError()
    QtS3Reply<T>::networkErrorString()
    QtS3Reply<T>::s3Error()
    QtS3Reply<T>::s3ErrorString()

Finally, the result can be obtained by calling value()

    T QtS3Reply<T>::value()

###Threading

The QtS3 accessor functions (get, but, exists, size) are thread-safe
and may be called from any thread. The QtS3 object itself should be
owned by one thread which creates and destroys it.

QtS3 signs upload requests. This includes computing a sha256 hash of
upload content and will use CPU according to content size. Using one
thread per request will parallelize these computations.

###Running tests

The "test" directory contains the autotest and testing data. A full test
run requires a key for S3 access and a couple of test buckets. These are
set using environment variables:

    QTS3_TEST_ACCESS_KEY_ID
    QTS3_TEST_SECRET_ACCESS_KEY
    QTS3_TEST_BUCKET_US
    QTS3_TEST_BUCKET_EU

(The test will skip some test cases if these are not set)
