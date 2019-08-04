/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef PULSEAUDIOSOURCE_H
#define PULSEAUDIOSOURCE_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QList>
#include <QTimer>
#include <QAudioInput>

extern "C" {
#include <pulse/mainloop.h>
#include <pulse/mainloop-signal.h>
#include <pulse/mainloop-api.h>
#include <pulse/context.h>
#include <pulse/error.h>
#include <pulse/introspect.h>
#include <pulse/subscribe.h>
#include <pulse/stream.h>
#include <pulse/xmalloc.h>
}

class PulseAudioSource : public QObject
{
    Q_OBJECT
public:
    PulseAudioSource(QObject *parent = nullptr);
    ~PulseAudioSource();
    bool start();
    void stop();
    void delayedStop();
    void resetDelayedStop();
    static pa_sample_spec sampleSpec;
    static bool inited();
    static void discoverStream();
    static bool encode(void *in_data, int in_size,
                       void **out_data, int *out_size);
private slots:
    void doPulseIteration();

private:
    static const char* nullSink;
    static const char* nullSinkMonitor;
    struct SinkInput {
        uint32_t idx = PA_INVALID_INDEX;
        uint32_t clientIdx = PA_INVALID_INDEX;
        uint32_t sinkIdx = PA_INVALID_INDEX;
        QString name;
        bool corked = false;
    };
    struct Client {
        uint32_t idx = PA_INVALID_INDEX;
        QString name;
        QString binary;
        QString icon;
    };
    static const int timerDelta; // msec
    static bool shutdown;
    QTimer iterationTimer;
    static int nullDataSize;
    static bool started;
    static bool timerActive;
    static bool muted;
    static pa_stream *stream;
    static uint32_t connectedSinkInput;
    static uint32_t connectedSink;
    static pa_mainloop *ml;
    static pa_mainloop_api *mla;
    static pa_context *ctx;
    static QHash<uint32_t, PulseAudioSource::Client> clients;
    static QHash<uint32_t, PulseAudioSource::SinkInput> sinkInputs;
    static QHash<uint32_t, QByteArray> monitorSources; // sink idx => source name
    static bool init();
    static void deinit();
    static void subscriptionCallback(pa_context *ctx, pa_subscription_event_type_t t, uint32_t idx, void *userdata);
    static void stateCallback(pa_context *ctx, void *userdata);
    static void successSubscribeCallback(pa_context *ctx, int success, void *userdata);
    static void streamRequestCallback(pa_stream *stream, size_t nbytes, void *userdata);
    static bool startRecordStream(pa_context *ctx, const SinkInput &si);
    static void stopRecordStream();
    static void sinkInputInfoCallback(pa_context *ctx, const pa_sink_input_info *i, int eol, void *userdata);
    static void sinkInfoCallback(pa_context *ctx, const pa_sink_info *i, int eol, void *userdata);
    static void clientInfoCallback(pa_context *ctx, const pa_client_info *i, int eol, void *userdata);
    static void timeEventCallback(pa_mainloop_api *mla, pa_time_event *e, const struct timeval *tv, void *userdata);
    static void contextSuccessCallback(pa_context *ctx, int success, void *userdata);
    static bool checkIfShouldBeEnabled();
    static void muteConnectedSinkInput(const SinkInput &si);
    static void unmuteConnectedSinkInput();
    static bool isBlacklisted(const char* name);
    static void correctClientName(Client &client);
    static QString subscriptionEventToStr(pa_subscription_event_type_t t);
    static QList<PulseAudioSource::Client> activeClients();
};

#endif // PULSEAUDIOSOURCE_H
