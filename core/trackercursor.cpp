#include <QDebug>

#include "trackercursor.h"

TrackerCursor::TrackerCursor(const QStringList& varNames, const QByteArray &data) :
    buffer(data.data()),
    buffer_size(uint32_t(data.size())),
    variable_names(varNames)
{
    _n_columns = varNames.length();
}

int TrackerCursor::columnCount()
{
    return _n_columns;
}

QVariant TrackerCursor::value(int column)
{
    if (column >= _n_columns)
        return QVariant(QVariant::Invalid);

    const char* value = (column == 0 ? data : data + offsets[column - 1] + 1);

    /*
        UNBOUND,
        URI,
        STRING,
        INTEGER,
        DOUBLE,
        DATETIME,
        BLANK_NODE,
        BOOLEAN
    */

    switch (type(column)) {
    case URI:
    case STRING:
    case DATETIME:
        return QVariant(QString(value));
    case INTEGER:
        return QVariant(QString(value).toInt());
    case DOUBLE:
        return QVariant(QString(value).toDouble());
    case BLANK_NODE:
        return QVariant(QVariant::String);
    default:
        return QVariant(QVariant::Invalid);
    }
}

TrackerCursor::ValueType TrackerCursor::type(int column)
{
    if (column >= _n_columns)
        return UNBOUND;

    return static_cast<TrackerCursor::ValueType>(types[column]);
}

QString TrackerCursor::name(int column)
{
    return variable_names.at(column);
}

bool TrackerCursor::isBound(int column)
{
    if (type(column) != UNBOUND)
        return true;

    return false;
}

bool TrackerCursor::next()
{
    int last_offset;

    if (buffer_index >= buffer_size) {
        return false;
    }

    /* So, the make up on each cursor segment is:
     *
     * iteration = [4 bytes for number of columns,
     *              columns x 4 bytes for types
     *              columns x 4 bytes for offsets]
     */

    _n_columns = buffer_read_int();

    types = (int*) (buffer + buffer_index);
    buffer_index += sizeof (int) * _n_columns;

    offsets = (int*) (buffer + buffer_index);
    buffer_index += sizeof (int) * (_n_columns - 1);
    last_offset = buffer_read_int ();

    data = buffer + buffer_index;

    buffer_index += last_offset + 1;

    return true;
}

void TrackerCursor::rewind()
{
    buffer_index = 0;
    data = buffer;
}
