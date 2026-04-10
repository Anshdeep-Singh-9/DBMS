#ifndef TUPLE_SERIALIZER_H
#define TUPLE_SERIALIZER_H

#include <cstdint>
#include <string>
#include <vector>

/*
 * Tuple serialization is the bridge between "row values" and "page bytes".
 *
 * Before:
 * - DataPage could only store raw bytes
 * - there was no clean way to convert actual row values into bytes
 *
 * After:
 * - rows can be packed into a consistent byte format
 * - the same format can later be unpacked back into values
 *
 * Layman version:
 * - this is the packing and unpacking rule for a row
 * - it tells us how to turn values into bytes and bytes back into values
 */

enum StorageColumnType {
    STORAGE_COLUMN_INT = 1,
    STORAGE_COLUMN_VARCHAR = 2
};

/*
 * ColumnSchema describes one column of a row for storage purposes.
 *
 * name       -> column name
 * type       -> integer or varchar
 * max_length -> varchar limit for V1, ignored for integer
 */
struct ColumnSchema {
    std::string name;
    uint16_t type;
    uint16_t max_length;

    ColumnSchema()
        : name(), type(STORAGE_COLUMN_INT), max_length(0) {
    }

    ColumnSchema(const std::string& column_name,
                 uint16_t column_type,
                 uint16_t column_max_length = 0)
        : name(column_name),
          type(column_type),
          max_length(column_max_length) {
    }
};

/*
 * TupleValue stores one row value in memory before serialization or after
 * deserialization.
 *
 * Layman version:
 * - this is one field value before it gets packed into bytes
 */
struct TupleValue {
    uint16_t type;
    int32_t int_value;
    std::string string_value;

    TupleValue()
        : type(STORAGE_COLUMN_INT), int_value(0), string_value() {
    }

    static TupleValue FromInt(int32_t value);
    static TupleValue FromVarchar(const std::string& value);
};

class TupleSerializer {
  public:
    static bool serialize(const std::vector<ColumnSchema>& schema,
                          const std::vector<TupleValue>& values,
                          std::vector<char>& tuple_out);

    static bool deserialize(const std::vector<ColumnSchema>& schema,
                            const std::vector<char>& tuple_bytes,
                            std::vector<TupleValue>& values_out);
};

#endif
