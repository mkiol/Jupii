
#include <QXmlStreamReader>
#include <QStringRef>
#include <QString>
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "yamahaextendedcontrol.h"
#include "directory.h"

const QString XCParser::urlBase_tag = "yamaha:X_URLBase";
const QString XCParser::controlUrl_tag = "yamaha:X_yxcControlURL";

XCParser::Data XCParser::parse(const QString &desc)
{
    Data data;
    QXmlStreamReader xml(desc);
    QString* elm = nullptr;

    while (!xml.atEnd()) {
        auto type = xml.readNext();
        if (type == QXmlStreamReader::StartElement) {
            if (xml.qualifiedName() == urlBase_tag)
                elm = &data.urlBase;
            else if (xml.qualifiedName() == controlUrl_tag)
                elm = &data.controlUrl;
        } else if (elm && type == QXmlStreamReader::Characters)
            *elm = xml.text().toString();
        else if (type == QXmlStreamReader::EndElement)
             elm = nullptr;
    }

    if (xml.hasError())
        qWarning() << "XML parsing error";

    //qDebug() << "YXC:" << urlBase << controlUrl;

    data.valid = !data.urlBase.isEmpty() && !data.controlUrl.isEmpty();

    return data;
}

YamahaXC::YamahaXC()
{
}

bool YamahaXC::valid()
{
    return !data.urlBase.isEmpty() && !data.controlUrl.isEmpty();
}

void YamahaXC::powerToggle()
{
    qDebug() << "Power toggle";

    if (valid()) {
        if (data.urlBase.endsWith('/'))
            data.urlBase = data.urlBase.left(data.urlBase.length() - 1);
        QUrl url(data.urlBase + data.controlUrl + "main/setPower?power=toggle");
        qDebug() << url.toString();
        Directory::instance()->nm->get(QNetworkRequest(url));
    }
}
