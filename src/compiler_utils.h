#pragma once
#include "numeric_types.h"

#define REF(field) ((void)field)

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

constexpr
size_t ConstexprStrlen(const char *String)
{
    return *String ? 1 + ConstexprStrlen(String + 1) : 0;
}

// fnv32 information from http://isthe.com/chongo/tech/comp/fnv/#FNV-1a
constexpr
uint32 Constexprfnv32(uint8 *Buffer, size_t Length, uint32 Hash, uint32 Prime)
{
    return Length ? Constexprfnv32(Buffer + 1, Length - 1, (Hash * Prime) ^ *Buffer, Prime) : Hash;
}

constexpr
uint32 Constexprfnv32(uint8 *Buffer, size_t Length)
{
    // values from  the link above
    return Constexprfnv32(Buffer, Length, 2166136261, 1677769);
}

constexpr
uint32 Constexprfnv32(const char *String)
{
    return Constexprfnv32((uint8 *)String, ConstexprStrlen(String));
}

