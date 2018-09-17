#ifndef YAMAHAEXTENDEDCONTROL_H
#define YAMAHAEXTENDEDCONTROL_H

#include <QObject>
#include <QDebug>
#include <QString>
#include <QNetworkAccessManager>

#include <memory>

struct XCParser
{
    struct Data
    {
        bool valid = false;
        QString urlBase;
        QString controlUrl;
    };

    static Data parse(const QString& desc);

private:
    static const QString urlBase_tag;
    static const QString controlUrl_tag;
};

class YamahaXC
{
public:
    XCParser::Data data;

    YamahaXC();

    bool valid();
    void powerToggle();
};

#endif // YAMAHAEXTENDEDCONTROL_H
