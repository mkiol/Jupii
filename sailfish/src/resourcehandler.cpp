#include "resourcehandler.h"
#include <dlfcn.h>
#include <QDebug>

#include "settings.h"

void grant_callback(resource_set_t *, uint32_t, void *) {}

static resource_set_t *(*create_set)(const char*,uint32_t,uint32_t, uint32_t,
                                     resource_callback_t, void *);
static int (*acquire_set)(resource_set_t*);
static int (*release_set)(resource_set_t*);

ResourceHandler::ResourceHandler(QObject *parent) :
    QObject(parent), m_hwEnabled(Settings::instance()->getUseHWVolume())
{
    create_set = (resource_set_t* (*)(const char*, uint32_t, uint32_t, uint32_t,
                 resource_callback_t, void*)) dlsym(nullptr, "resource_set_create");
    acquire_set = (int (*)(resource_set_t*))dlsym(nullptr, "resource_set_acquire");
    release_set = (int (*)(resource_set_t*))dlsym(nullptr, "resource_set_release");
    if (create_set && acquire_set && release_set) {
        m_resource = create_set("player", RESOURCE_SCALE_BUTTON, 0, 0,
                                grant_callback, nullptr);
    } else {
        qDebug() << "Cannot grant resources for HW keys";
    }
}

void ResourceHandler::acquire()
{    
    if (m_resource)
        acquire_set(m_resource);
}

void ResourceHandler::release()
{
    if (m_resource)
        release_set(m_resource);
}

void ResourceHandler::handleFocusChange(QObject *focus)
{
    //qDebug() << "handleFocusChange" << focus;
    if (focus) {
        m_aquired = true;
        if (m_hwEnabled)
            acquire();
    } else {
        m_aquired = false;
        release();
    }
}

void ResourceHandler::handleSettingsChange()
{
    m_hwEnabled = Settings::instance()->getUseHWVolume();

    if (m_hwEnabled) {
        if (m_aquired)
            acquire();
    } else {
        release();
    }
}
