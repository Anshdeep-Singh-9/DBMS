#include "tuple_serializer.h"

#include <cstring>
#include <limits>

namespace {
template <typename T>
void append_value(std::vector<char>& out, const T& value) {
    const char* start = reinterpret_cast<const char*>(&value);
    out.insert(out.end(), start, start + sizeof(T));
}

template <typename T>
bool read_value(const std::vector<char>& bytes,
                std::size_t offset,
                T& value_out) {
    if (offset + sizeof(T) > bytes.size()) {
        return false;
    }

    std::memcpy(&value_out, &bytes[offset], sizeof(T));
    return true;
}
}  // namespace

TupleValue TupleValue::FromInt(int32_t value) {
    TupleValue tuple_value;
    tuple_value.type = STORAGE_COLUMN_INT;
    tuple_value.int_value = value;
    return tuple_value;
}

TupleValue TupleValue::FromVarchar(const std::string& value) {
    TupleValue tuple_value;
    tuple_value.type = STORAGE_COLUMN_VARCHAR;
    tuple_value.string_value = value;
    return tuple_value;
}

bool TupleSerializer::serialize(const std::vector<ColumnSchema>& schema,
                                const std::vector<TupleValue>& values,
                                std::vector<char>& tuple_out) {
    tuple_out.clear();

    if (schema.size() != values.size()) {
        return false;
    }

    for (std::size_t i = 0; i < schema.size(); ++i) {
        const ColumnSchema& column = schema[i];
        const TupleValue& value = values[i];

        if (column.type != value.type) {
            return false;
        }

        if (column.type == STORAGE_COLUMN_INT) {
            append_value(tuple_out, value.int_value);
            continue;
        }

        if (column.type == STORAGE_COLUMN_VARCHAR) {
            if (value.string_value.size() >
                static_cast<std::size_t>(column.max_length)) {
                return false;
            }

            if (value.string_value.size() >
                static_cast<std::size_t>(std::numeric_limits<uint16_t>::max())) {
                return false;
            }

            const uint16_t string_length =
                static_cast<uint16_t>(value.string_value.size());
            append_value(tuple_out, string_length);
            tuple_out.insert(tuple_out.end(),
                             value.string_value.begin(),
                             value.string_value.end());
            continue;
        }

        return false;
    }

    return true;
}

bool TupleSerializer::deserialize(const std::vector<ColumnSchema>& schema,
                                  const std::vector<char>& tuple_bytes,
                                  std::vector<TupleValue>& values_out) {
    values_out.clear();

    std::size_t offset = 0;
    for (std::size_t i = 0; i < schema.size(); ++i) {
        const ColumnSchema& column = schema[i];

        if (column.type == STORAGE_COLUMN_INT) {
            int32_t int_value = 0;
            if (!read_value(tuple_bytes, offset, int_value)) {
                return false;
            }

            values_out.push_back(TupleValue::FromInt(int_value));
            offset += sizeof(int32_t);
            continue;
        }

        if (column.type == STORAGE_COLUMN_VARCHAR) {
            uint16_t string_length = 0;
            if (!read_value(tuple_bytes, offset, string_length)) {
                return false;
            }

            offset += sizeof(uint16_t);
            if (offset + string_length > tuple_bytes.size()) {
                return false;
            }

            if (string_length > column.max_length) {
                return false;
            }

            std::string string_value(&tuple_bytes[offset],
                                     &tuple_bytes[offset] + string_length);
            values_out.push_back(TupleValue::FromVarchar(string_value));
            offset += string_length;
            continue;
        }

        return false;
    }

    return offset == tuple_bytes.size();
}
