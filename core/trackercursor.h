#ifndef TRACKERCURSOR_H
#define TRACKERCURSOR_H

#include <QStringList>
#include <QByteArray>
#include <QVariant>
#include <QString>

class TrackerCursor
{
public:
    enum ValueType {
        UNBOUND,
        URI,
        STRING,
        INTEGER,
        DOUBLE,
        DATETIME,
        BLANK_NODE,
        BOOLEAN
    };

    TrackerCursor(const QStringList& varNames, const QByteArray& data);

    int columnCount();
    bool isBound(int column);
    QString name(int column);
    ValueType type(int column);
    QVariant value(int column);

    bool next();
    void rewind();

private:
    const char* buffer;
    ulong buffer_size;
    ulong buffer_index = 0;

    int _n_columns;
    int* offsets;
    int* types;
    const char* data;
    QStringList variable_names;

    inline int buffer_read_int()
    {
        int v = *((int*) (buffer + buffer_index));
        buffer_index += 4;

        return v;
    }
};

#endif // TRACKERCURSOR_H
