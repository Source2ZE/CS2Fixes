#include "schema.h"

#include "../common.h"
#include "interfaces/cs2_interfaces.h"
//#include <unordered_map>
#include "tier1/utlmap.h"

#include "tier0/memdbgon.h"

using SchemaKeyValueMap_t = CUtlMap<uint32_t, int16_t>;
using SchemaTableMap_t = CUtlMap<uint32_t, SchemaKeyValueMap_t*>;

static bool InitSchemaFieldsForClass(SchemaTableMap_t& tableMap, const char* className, uint32_t classKey)
{
    CSchemaSystemTypeScope* pType = interfaces::pSchemaSystem->FindTypeScopeForModule("server.dll");

    if (!pType)
        return false;

    SchemaClassInfoData_t* pClassInfo = pType->FindDeclaredClass(className);
    if (!pClassInfo)
    {
        //tableMap.emplace(classKey, SchemaKeyValueMap_t{});
        SchemaKeyValueMap_t *map = new SchemaKeyValueMap_t;
        tableMap.Insert(classKey, map);

        Warning("InitSchemaFieldsForClass(): '%s' was not found!\n", className);
        return false;
    }

    short fieldsSize = pClassInfo->GetFieldsSize();
    SchemaClassFieldData_t* pFields = pClassInfo->GetFields();

    auto& keyValueMap = tableMap[classKey];
    //keyValueMap.reserve(fieldsSize);
    

    for (int i = 0; i < fieldsSize; ++i)
    {
        SchemaClassFieldData_t& field = pFields[i];

#ifndef CS2_SDK_ENABLE_SCHEMA_FIELD_OFFSET_LOGGING
        Message("%s::%s found at -> 0x%X - %llx\n", className, field.m_name, field.m_offset, &field);
#endif

        keyValueMap->Insert(hash_32_fnv1a_const(field.m_name), field.m_offset);
    }

    return true;
}

int16_t schema::FindChainOffset(const char* className)
{
    CSchemaSystemTypeScope* pType = interfaces::pSchemaSystem->FindTypeScopeForModule("server.dll");

    if (!pType)
        return false;

    SchemaClassInfoData_t* pClassInfo = pType->FindDeclaredClass(className);

    do
    {
        SchemaClassFieldData_t* pFields = pClassInfo->GetFields();
        short fieldsSize = pClassInfo->GetFieldsSize();
        for (int i = 0; i < fieldsSize; ++i)
        {
            SchemaClassFieldData_t& field = pFields[i];

            if (V_strcmp(field.m_name, "__m_pChainEntity") == 0)
            {
                return field.m_offset;
            }
        }
    } while ((pClassInfo = pClassInfo->GetParent()) != nullptr);

    return 0;
}

int16_t schema::GetOffset(const char* className, uint32_t classKey, const char* memberName, uint32_t memberKey)
{
    static SchemaTableMap_t schemaTableMap;
    const auto& tableMapIndex = schemaTableMap.Find(classKey);
    if (!schemaTableMap.IsValidIndex(tableMapIndex)) // is this correct lol? It's checking if the key is not there and if not it calls below
    {
        if (InitSchemaFieldsForClass(schemaTableMap, className, classKey))
            return GetOffset(className, classKey, memberName, memberKey);

        return 0;
    }

    const SchemaKeyValueMap_t* tableMap = schemaTableMap[tableMapIndex];
    const auto& memberIndex = tableMap->Find(memberKey);
    if (!tableMap->IsValidIndex(memberIndex))
    {
        Warning("schema::GetOffset(): '%s' was not found in '%s'!\n", memberName, className);
        return 0;
    }

    return (*tableMap)[memberIndex];
}
