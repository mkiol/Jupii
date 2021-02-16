/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef GUMBOTOOLS_H
#define GUMBOTOOLS_H

#include <QString>
#include <QByteArray>
#include <memory>
#include <functional>
#include <vector>

#include "gumbo.h"

namespace gumbo
{
typedef std::unique_ptr<GumboOutput, std::function<void (GumboOutput*)>> GumboOutput_ptr;
GumboOutput_ptr parseHtmlData(const QByteArray &data);
void search_for_class(GumboNode* node, const char* cls_name, std::vector<GumboNode*> *nodes);
std::vector<GumboNode*> search_for_class(GumboNode* node, const char* cls_name);
GumboNode* search_for_class_one(GumboNode* node, const char* cls_name);
GumboNode* search_for_id(GumboNode* node, const char* id);
GumboNode* search_for_attr_one(GumboNode* node, const char* attr_name);
GumboNode* search_for_attr_one(GumboNode* node, const char* attr_name, const char* attr_value);
GumboNode* search_for_tag_one(GumboNode* node, GumboTag tag);
void search_for_tag(GumboNode* node, GumboTag tag, std::vector<GumboNode*> *nodes);
std::vector<GumboNode*> search_for_tag(GumboNode* node, GumboTag tag);
QString node_text(GumboNode* node);
QByteArray attr_data(GumboNode* node, const char *attr_name);
}

#endif // GUMBOTOOLS_H
