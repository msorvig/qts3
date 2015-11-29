#include <QtCore>

#include <qts3.h>

#ifdef USE_QPM_NS
using namespace com::github::msorvig::s3;
#endif

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QByteArray accessKeyId = qgetenv("AWS_S3_ACCESS_KEY_ID");
    QByteArray secretAccessKey = qgetenv("AWS_S3_SECRET_ACCESS_KEY");

    QByteArray bucketName = "testbucket";
    QByteArray objectName = "testfile";
    
    QtS3 s3(accessKeyId, secretAccessKey);

    QFile f("example.cpp");
    f.open(QIODevice::ReadOnly);
    QByteArray contents = f.readAll();
    
    qDebug() << "Uploading file bucket" << bucketName;
    QtS3Reply<void> putReply = s3.put(bucketName, objectName, contents);
    if (!putReply.isSuccess())
        qDebug() << "S3 put error" << putReply.anyErrorString();
        
    qDebug() << "Checking object size";
    QtS3Reply<int> sizeReply = s3.size(bucketName, objectName);
    if (!sizeReply.isSuccess())
        qDebug() << "S3 size error:" << sizeReply.anyErrorString();
    else
        qDebug() << "Object size is:" << sizeReply.value();
}

    