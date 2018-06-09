#include "services.h"

Services::Services(QObject* parent) :
    QObject(parent),
    renderingControl(new RenderingControl(parent)),
    avTransport(new AVTransport(parent))
{
}
