#ifndef SERVICES_H
#define SERVICES_H

#include <QObject>

#include <memory>

class RenderingControl;
class AVTransport;
class ContentDirectory;

class Services : public QObject
{
    Q_OBJECT
public:
    static Services* instance(QObject *parent = nullptr);

    std::shared_ptr<RenderingControl> renderingControl;
    std::shared_ptr<AVTransport> avTransport;
    std::shared_ptr<ContentDirectory> contentDir;

private:
    static Services* m_instance;
    explicit Services(QObject* parent = nullptr);
};

#endif // SERVICES_H
