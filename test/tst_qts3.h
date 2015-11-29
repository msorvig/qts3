#ifndef QTS3_TST_P_H
#define QTS3_TST_P_H

#include <QtCore/QtCore>

//  Test data from:
//  http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
//  http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html

namespace AwsTestData
{
// a consistent data set
static const QDateTime timeStamp(QDate(2011, 9, 9), QTime(23, 36, 0));
static const QByteArray accessKeyId = "AKIDEXAMPLE";
static const QByteArray secretAccessKey = "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";
static const QByteArray date = "20110909";
static const QByteArray dateTime = "20110909T233600Z";
static const QByteArray region = "us-east-1";
static const QByteArray host = "iam.amazonaws.com";
static const QByteArray service = "iam";
static const QByteArray signingKey =
    "98f1d889fec4f4421adc522bab0ce1f82e6929c262ed15e5a94c90efd1e3b0e7";
static const QByteArray method = "POST";
static const QByteArray url = "/";
static const QByteArray queryString = "";
static const QByteArray content = "Action=ListUsers&Version=2010-05-08";
static const QByteArray contentHash =
    "b6359072c78d70ebee1e81adcbab4f01bf2c23245fa365ef83fe8f1f955085e2";
static const QByteArray canonicalRequest =
    ""
    "POST\n"
    "/\n"
    "\n"
    "content-type:application/x-www-form-urlencoded; charset=utf-8\n"
    "host:iam.amazonaws.com\n"
    "x-amz-date:20110909T233600Z\n"
    "\n"
    "content-type;host;x-amz-date\n"
    "b6359072c78d70ebee1e81adcbab4f01bf2c23245fa365ef83fe8f1f955085e2";
static const QByteArray canonicalRequestHash =
    "3511de7e95d28ecd39e9513b642aee07e54f4941150d8df8bf94b328ef7e55e2";
static const QByteArray stringToSign =
    "AWS4-HMAC-SHA256\n"
    "20110909T233600Z\n"
    "20110909/us-east-1/iam/aws4_request\n"
    "3511de7e95d28ecd39e9513b642aee07e54f4941150d8df8bf94b328ef7e55e2";
static const QHash<QByteArray, QByteArray> headers = {
    {"Host", "iam.amazonaws.com"},
    {"Content-Type", "application/x-www-form-urlencoded; charset=utf-8"},
    {"X-Amz-Date", "20110909T233600Z"}};
static const QByteArray signedHeaders = "content-type;host;x-amz-date";
static const QByteArray signature =
    "ced6826de92d2bdeed8f846f0bf508e8559e98e4b0199114b84c54174deb456c";
static const QByteArray authorizationHeaderValue =
    "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20110909/us-east-1/iam/aws4_request, "
    "SignedHeaders=content-type;host;x-amz-date, "
    "Signature=ced6826de92d2bdeed8f846f0bf508e8559e98e4b0199114b84c54174deb456c";

// a second consistent data set
namespace presignedUrl {
    static const QDateTime timeStamp(QDate(2013, 5, 24), QTime(23, 36, 0));
    static const QByteArray accessKeyId	= "AKIAIOSFODNN7EXAMPLE";
    static const QByteArray secretAccessKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
    static const QByteArray canonicalRequest =
        "GET\n"
        "/test.txt\n"
        "X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIAIOSFODNN7EXAMPLE%2F20130524%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20130524T000000Z&X-Amz-Expires=86400&X-Amz-SignedHeaders=host\n"
        "host:examplebucket.s3.amazonaws.com\n"
        "\n"
        "host\n"
        "UNSIGNED-PAYLOAD";
    static const QByteArray stringToSign =
        "AWS4-HMAC-SHA256"
        "20130524T000000Z"
        "20130524/us-east-1/s3/aws4_request"
        "3bfa292879f6447bbcda7001decf97f4a54dc650c8942174ae0a9121cf58ad04";
    static const QByteArray signature =
        "aeeed9bbccd4d02ee5c0109b86d86835f995330da4c265957d157751f604d404";
    static const QByteArray presignedUrl =
        "https://examplebucket.s3.amazonaws.com/test.txt?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIAIOSFODNN7EXAMPLE%2F20130524%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20130524T000000Z&X-Amz-Expires=86400&X-Amz-SignedHeaders=host&X-Amz-Signature=aeeed9bbccd4d02ee5c0109b86d86835f995330da4c265957d157751f604d404";

}

// extra test data not part of the consistent data set
static const QByteArray inputQueryString =
    "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
    "X-Amz-Credential=AKIAIOSFODNN7EXAMPLE%2F20110909/us-east-1/iam/aws4_request&"
    "X-Amz-Date=20110909T233600Z&"
    "X-Amz-SignedHeaders=content-type;host;x-amz-date&"
    "Action=ListUsers&"
    "Version=2010-05-08";
static const QByteArray canonicalQueryString =
    "Action=ListUsers&"
    "Version=2010-05-08&"
    "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
    "X-Amz-Credential=AKIAIOSFODNN7EXAMPLE%2F20110909%2Fus-east-1%2Fiam%2Faws4_request&"
    "X-Amz-Date=20110909T233600Z&"
    "X-Amz-SignedHeaders=content-type%3Bhost%3Bx-amz-date";
};

#endif
