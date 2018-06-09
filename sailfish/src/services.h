#ifndef SERVICES_H
#define SERVICES_H

#include <QObject>

#include <memory>

#include "renderingcontrol.h"
#include "avtransport.h"

class Services : public QObject
{
    Q_OBJECT
public:
    explicit Services(QObject* parent = nullptr);

    std::shared_ptr<RenderingControl> renderingControl;
    std::shared_ptr<AVTransport> avTransport;
};

#endif // SERVICES_H
