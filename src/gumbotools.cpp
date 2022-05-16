/* Copyright (C) 2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "gumbotools.h"

#include <QTextStream>

namespace gumbo {

GumboOutput_ptr parseHtmlData(const QByteArray &data)
{
    return GumboOutput_ptr(
                gumbo_parse_with_options(&kGumboDefaultOptions, data.data(),
                size_t(data.length())), [](auto output){
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    });
}

void search_for_class(GumboNode* node, const char* cls_name, std::vector<GumboNode*> *nodes)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return;
    }

    GumboAttribute* cls_attr;
    if ((cls_attr = gumbo_get_attribute(&node->v.element.attributes, "class")) &&
            strstr(cls_attr->value, cls_name) != nullptr) {
        nodes->push_back(node);
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
        search_for_class(static_cast<GumboNode*>(children->data[i]), cls_name, nodes);
    }
}

std::vector<GumboNode*> search_for_class(GumboNode* node, const char* cls_name)
{
    std::vector<GumboNode*> nodes;
    search_for_class(node, cls_name, &nodes);
    return nodes;
}

GumboNode* search_for_attr_one(GumboNode* node, const char* attr_name, const char* attr_value)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }

    GumboAttribute* attr;
    if ((attr = gumbo_get_attribute(&node->v.element.attributes, attr_name)) &&
            strstr(attr->value, attr_value) != nullptr) {
        return node;
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
        GumboNode* result = search_for_attr_one(static_cast<GumboNode*>(children->data[i]), attr_name, attr_value);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

GumboNode* search_for_class_one(GumboNode* node, const char* cls_name)
{
    return search_for_attr_one(node, "class", cls_name);
}

GumboNode* search_for_id(GumboNode* node, const char* id)
{
    return search_for_attr_one(node, "id", id);
}

GumboNode* search_for_attr_one(GumboNode* node, const char* attr_name)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }

    if (gumbo_get_attribute(&node->v.element.attributes, attr_name) != nullptr) {
        return node;
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
        GumboNode* result = search_for_attr_one(static_cast<GumboNode*>(children->data[i]), attr_name);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

void search_for_tag(GumboNode* node, GumboTag tag, std::vector<GumboNode*> *nodes)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return;
    }

    if (node->v.element.tag == tag) {
        nodes->push_back(node);
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
        search_for_tag(static_cast<GumboNode*>(children->data[i]), tag, nodes);
    }
}

std::vector<GumboNode*> search_for_tag(GumboNode* node, GumboTag tag)
{
    std::vector<GumboNode*> nodes;
    search_for_tag(node, tag, &nodes);
    return nodes;
}

GumboNode* search_for_tag_one(GumboNode* node, GumboTag tag)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }

    if (node->v.element.tag == tag) {
        return node;
    }

    GumboVector* children = &node->v.element.children;
    for (size_t i = 0; i < children->length; ++i) {
        GumboNode* result = search_for_tag_one(static_cast<GumboNode*>(children->data[i]), tag);
        if (result) {
            return result;
        }
    }

    return nullptr;
}

QString node_text(GumboNode* node)
{
    QString text;
    QTextStream ts(&text);

    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }

    for (size_t i = 0; i < node->v.element.children.length; ++i) {
        auto text = static_cast<GumboNode*>(node->v.element.children.data[0]);
        if (text->type == GUMBO_NODE_TEXT) {
            ts << text->v.text.text;
        }
    }

    return text.simplified();
}

QByteArray attr_data(GumboNode* node, const char* attr_name)
{
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }

    GumboAttribute* attr_value;
    if ((attr_value = gumbo_get_attribute(&node->v.element.attributes, attr_name)) != nullptr) {
        return QByteArray(attr_value->value);
    }

    return {};
}
}
