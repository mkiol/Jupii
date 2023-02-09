#include "resourcehandler.h"

#include <dlfcn.h>

#include <QDebug>

#include "renderingcontrol.h"
#include "services.h"
#include "settings.h"

void grant_callback(resource_set_t *, uint32_t, void *) {}

static resource_set_t *(*create_set)(const char *, uint32_t, uint32_t, uint32_t,
                                     resource_callback_t, void *);
static int (*acquire_set)(resource_set_t *);
static int (*release_set)(resource_set_t *);

ResourceHandler::ResourceHandler(QObject *parent)
    : QObject(parent), m_enabled(Settings::instance()->getUseHWVolume()) {
    // Copied from
    // https://github.com/piggz/harbour-advanced-camera/blob/master/src/resourcehandler.cpp

    auto handle = dlopen("libresource-glib.so.0", RTLD_LAZY);
    if (handle) {
        create_set =
            (resource_set_t * (*)(const char *, uint32_t, uint32_t, uint32_t,
                                  resource_callback_t, void *))
                dlsym(handle, "resource_set_create");
        if (!create_set) {
            qDebug() << dlerror();
        }
        acquire_set =
            (int (*)(resource_set_t *))dlsym(handle, "resource_set_acquire");
        if (!acquire_set) {
            qDebug() << dlerror();
        }
        release_set =
            (int (*)(resource_set_t *))dlsym(handle, "resource_set_release");
        if (!release_set) {
            qDebug() << dlerror();
        }
        if (create_set && acquire_set && release_set) {
            m_resource = create_set("player", RESOURCE_SCALE_BUTTON, 0, 0,
                                    &grant_callback, nullptr);
            if (m_resource) {
                connectHandlers();
                handleSettingsChange();
                return;
            }
        }
    }

    qWarning() << "failed to grant resources for hw keys";
}

void ResourceHandler::connectHandlers() {
    auto *rc = Services::instance()->renderingControl.get();
    auto *s = Settings::instance();
    connect(s, &Settings::useHWVolumeChanged, this,
            &ResourceHandler::handleSettingsChange);
    connect(rc, &Service::initedChanged, this,
            &ResourceHandler::handleSettingsChange);
}

void ResourceHandler::acquire() {
    if (m_resource) acquire_set(m_resource);
}

void ResourceHandler::release() {
    if (m_resource) release_set(m_resource);
}

void ResourceHandler::handleFocusChange(QObject *focus) {
    if (focus) {
        m_aquired = true;
        if (m_enabled) acquire();
    } else {
        m_aquired = false;
        release();
    }
}

void ResourceHandler::handleSettingsChange() {
    auto *rc = Services::instance()->renderingControl.get();
    auto *s = Settings::instance();

    m_enabled = s->getUseHWVolume() && rc->getInited();

    if (m_enabled) {
        if (m_aquired) acquire();
    } else {
        release();
    }
}
