/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "radionetmodel.hpp"

#include <QDebug>
#include <vector>

#include "iconprovider.h"
#include "radionetapi.hpp"

RadionetModel::RadionetModel(QObject *parent)
    : SelectableItemModel(new RadionetItem, parent) {}

QVariantList RadionetModel::selectedItems() {
    QVariantList list;

    list.reserve(selectedCount());

    std::for_each(m_list.cbegin(), m_list.cend(), [&](const auto *item) {
        const auto *station = qobject_cast<const RadionetItem *>(item);
        if (station->selected()) {
            list.push_back(QVariantMap{
                {QStringLiteral("url"), {station->url()}},
                {QStringLiteral("name"), {station->name()}},
                {QStringLiteral("icon"), {station->icon()}},
                {QStringLiteral("author"), {QStringLiteral("radio.net")}},
                {QStringLiteral("app"), {QStringLiteral("radionet")}}});
        }
    });

    return list;
}

static auto formatToStr(RadionetApi::Format format) {
    switch (format) {
        case RadionetApi::Format::Mp3:
            return QStringLiteral("MP3");
        case RadionetApi::Format::Aac:
            return QStringLiteral("AAC");
        case RadionetApi::Format::Hls:
            return QStringLiteral("HLS");
        default:
            return QString{};
    }
}

bool RadionetModel::canShowMore() const {
    return RadionetApi::canMakeMoreLocalItems();
}

QList<ListItem *> RadionetModel::makeItems() {
    QList<ListItem *> items;

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    std::vector<RadionetApi::Item> entries;

    auto filter = getFilter();

    RadionetApi api{};

    if (filter.isEmpty()) {
        if (m_showMoreRequested) {
            m_showMoreRequested = false;
            if (api.makeMoreLocalItems()) emit canShowMoreChanged();
        }
        entries = api.local();
    } else {
        m_lastIndex = 0;
        entries = api.search(filter);
    }

    if (entries.empty()) return items;

    if (QThread::currentThread()->isInterruptionRequested()) return items;

    items.reserve(entries.size());

    for (auto &entry : entries) {
        items.push_back(new RadionetItem{
            /*id=*/std::move(entry.id),
            /*name=*/std::move(entry.name),
            /*country=*/std::move(entry.country),
            /*city=*/std::move(entry.city),
            /*format=*/formatToStr(entry.format),
            /*genres=*/{},
            /*icon=*/
            entry.imageUrl.value_or(
                IconProvider::urlToNoResId(QStringLiteral("icon-radionet"))),
            /*url=*/std::move(entry.streamUrl)});
    }

    return items;
}

RadionetItem::RadionetItem(QString id, QString name, QString country,
                           QString city, QString format, QStringList genres,
                           QUrl icon, QUrl url, QObject *parent)
    : SelectableItem(parent), m_id{std::move(id)}, m_name{std::move(name)},
      m_country{std::move(country)}, m_city{std::move(city)},
      m_format{std::move(format)}, m_genres{std::move(genres)},
      m_icon{std::move(icon)}, m_url{std::move(url)} {}

QHash<int, QByteArray> RadionetItem::roleNames() const {
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[CountryRole] = "country";
    names[CityRole] = "city";
    names[FormatRole] = "format";
    names[GenresRole] = "genres";
    names[IconRole] = "icon";
    names[SelectedRole] = "selected";
    return names;
}

QVariant RadionetItem::data(int role) const {
    switch (role) {
        case IdRole:
            return id();
        case NameRole:
            return name();
        case CountryRole:
            return country();
        case CityRole:
            return city();
        case FormatRole:
            return format();
        case GenresRole:
            return genres();
        case IconRole:
            return icon();
        case SelectedRole:
            return selected();
        default:
            return {};
    }
}
