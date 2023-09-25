#pragma once
#pragma warning (push)
#pragma warning (disable : 4005)
#include <type_traits>
#pragma warning (pop)

#include "tier0/dbg.h"
#include "../addresses.h"
#undef schema

constexpr uint32_t val_32_const = 0x811c9dc5;
constexpr uint32_t prime_32_const = 0x1000193;
constexpr uint64_t val_64_const = 0xcbf29ce484222325;
constexpr uint64_t prime_64_const = 0x100000001b3;

inline constexpr uint32_t hash_32_fnv1a_const(const char* const str, const uint32_t value = val_32_const) noexcept {
    return (str[0] == '\0') ? value : hash_32_fnv1a_const(&str[1], (value ^ uint32_t(str[0])) * prime_32_const);
}

inline constexpr uint64_t hash_64_fnv1a_const(const char* const str, const uint64_t value = val_64_const) noexcept {
    return (str[0] == '\0') ? value : hash_64_fnv1a_const(&str[1], (value ^ uint64_t(str[0])) * prime_64_const);
}

#define SCHEMA_FIELD_OFFSET(varName, datatable, propName, extra_offset, type)  \
    std::add_lvalue_reference_t<type> varName() {                              \
        static constexpr auto datatable_hash = hash_32_fnv1a_const(datatable); \
        static constexpr auto prop_hash = hash_32_fnv1a_const(propName);       \
                                                                               \
        static const auto m_offset =                                           \
            schema::GetOffset(datatable, datatable_hash, propName, prop_hash); \
        ConMsg("Offset: %llx, address: %llx\n", m_offset, this);               \
                                                                               \
        return *reinterpret_cast<std::add_pointer_t<type>>(                    \
            (uintptr_t)(this) + m_offset + extra_offset);                      \
    }                                                                          \
    void varName##_update() {                                                  \
        static constexpr auto datatable_hash = hash_32_fnv1a_const(datatable); \
        static constexpr auto prop_hash = hash_32_fnv1a_const(propName);       \
                                                                               \
        static const auto m_offset =                                           \
            schema::GetOffset(datatable, datatable_hash, propName, prop_hash); \
                                                                               \
        static const auto m_chain =                                            \
             schema::FindChainOffset(datatable);                               \
																			   \
        ConMsg("Offsettest: %llx, offset2: %llx\n", m_chain, m_offset);            \
                                                                               \
        if(m_chain != 0)                                                       \
        {                                                                      \
            addresses::NetworkStateChanged((uintptr_t)(this) + m_chain, m_offset + extra_offset, 0xFFFFFFFF); \
        }                                                                      \
    }                                                                          



#define SCHEMA_FIELD(varName, datatable, propName, type) \
    SCHEMA_FIELD_OFFSET(varName, datatable, propName, 0, type)

#define PSCHEMA_FIELD_OFFSET(varName, datatable, propName, extra_offset, type) \
    auto varName() {                                                           \
        static constexpr auto datatable_hash = hash_32_fnv1a_const(datatable); \
        static constexpr auto prop_hash = hash_32_fnv1a_const(propName);       \
                                                                               \
        static const auto m_offset =                                           \
            schema::GetOffset(datatable, datatable_hash, propName, prop_hash); \
                                                                               \
        return reinterpret_cast<std::add_pointer_t<type>>(                     \
            (uintptr_t)(this) + m_offset + extra_offset);                      \
    }

#define PSCHEMA_FIELD(varName, datatable, propName, type) \
    PSCHEMA_FIELD_OFFSET(varName, datatable, propName, 0, type)

#define SCHEMA_FIELD_NEW(varName, datatable, extra_offset, type)                                                    \
    class varName##_prop                                                                                            \
    {                																			                    \
    public:                                                                                                         \
        std::add_lvalue_reference_t<type> Get()                                                                     \
        {                                                                                                           \
            static constexpr auto datatable_hash = hash_32_fnv1a_const(datatable);                                  \
            static constexpr auto prop_hash = hash_32_fnv1a_const(#varName);                                        \
                                                                                                                    \
            static const auto m_offset =                                                                            \
                schema::GetOffset(datatable, datatable_hash, #varName, prop_hash);                                  \
            ConMsg("Offset: %llx, address: %llx\n", m_offset, this);                                                \
                                                                                                                    \
            return *reinterpret_cast<std::add_pointer_t<type>>(                                                     \
                (uintptr_t)(this) + m_offset + extra_offset);                                                       \
        }                                                                                                           \
                                                                                                                    \
        void Set(type val)                                                                                          \
        {                                                                                                           \
            static constexpr auto datatable_hash = hash_32_fnv1a_const(datatable);                                  \
            static constexpr auto prop_hash = hash_32_fnv1a_const(#varName);                                        \
                                                                                                                    \
            static const auto m_offset =                                                                            \
                schema::GetOffset(datatable, datatable_hash, #varName, prop_hash);                                  \
                                                                                                                    \
            static const auto m_chain =                                                                             \
                schema::FindChainOffset(datatable);                                                                 \
																			                                        \
            ConMsg("Offset: %llx, offset2: %llx\n", m_chain, m_offset);                                             \
                                                                                                                    \
            if(m_chain != 0)                                                                                        \
            {                                                                                                       \
                addresses::NetworkStateChanged((uintptr_t)(this) + m_chain, m_offset + extra_offset, 0xFFFFFFFF);   \
            }                                                                                                       \
            *reinterpret_cast<std::add_pointer_t<type>>((uintptr_t)(this) + m_offset + extra_offset) = val;         \
        }                                                                                                           \
    } varName;                                                                                                      \

namespace schema {
    int16_t FindChainOffset(const char* className);
    int16_t GetOffset(const char* className, uint32_t classKey,
        const char* memberName, uint32_t memberKey);
}