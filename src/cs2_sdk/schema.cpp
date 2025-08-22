/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "schema.h"

#include "../common.h"
#include "entity/cbaseentity.h"
#include "plat.h"
#include "schemasystem/schemasystem.h"

#include "tier0/memdbgon.h"

using SchemaKeyValueMap_t = std::map<uint32_t, SchemaKey>;
using SchemaTableMap_t = std::map<uint32_t, SchemaKeyValueMap_t>;

static constexpr uint32_t g_ChainKey = hash_32_fnv1a_const("__m_pChainEntity");

static bool IsFieldNetworked(SchemaClassFieldData_t& field)
{
	for (int i = 0; i < field.m_nStaticMetadataCount; i++)
	{
		static auto networkEnabled = hash_32_fnv1a_const("MNetworkEnable");
		if (networkEnabled == hash_32_fnv1a_const(field.m_pStaticMetadata[i].m_pszName))
			return true;
	}

	return false;
}

// Try to recursively find __m_pChainEntity in base classes
// (e.g. CCSGameRules -> CTeamplayRules -> CMultiplayRules -> CGameRules, in this case it's in CGameRules)
static void InitChainOffset(SchemaClassInfoData_t* pClassInfo, SchemaKeyValueMap_t& keyValueMap)
{
	short fieldsSize = pClassInfo->m_nFieldCount;
	SchemaClassFieldData_t* pFields = pClassInfo->m_pFields;

	for (int i = 0; i < fieldsSize; ++i)
	{
		SchemaClassFieldData_t& field = pFields[i];

		if (hash_32_fnv1a_const(field.m_pszName) != g_ChainKey)
			continue;

		std::pair<uint32_t, SchemaKey> keyValuePair;
		keyValuePair.first = g_ChainKey;
		keyValuePair.second.offset = field.m_nSingleInheritanceOffset;
		keyValuePair.second.networked = IsFieldNetworked(field);

		keyValueMap.insert(keyValuePair);
		return;
	}

	// Not the base class yet, keep looking
	if (pClassInfo->m_nBaseClassCount)
		return InitChainOffset(pClassInfo->m_pBaseClasses[0].m_pClass, keyValueMap);
}

static void InitSchemaKeyValueMap(SchemaClassInfoData_t* pClassInfo, SchemaKeyValueMap_t& keyValueMap)
{
	short fieldsSize = pClassInfo->m_nFieldCount;
	SchemaClassFieldData_t* pFields = pClassInfo->m_pFields;

	for (int i = 0; i < fieldsSize; ++i)
	{
		SchemaClassFieldData_t& field = pFields[i];

#ifdef _DEBUG
		Message("%s::%s found at -> 0x%X - %llx\n", pClassInfo->m_pszName, field.m_pszName, field.m_nSingleInheritanceOffset, &field);
#endif

		std::pair<uint32_t, SchemaKey> keyValuePair;
		keyValuePair.first = hash_32_fnv1a_const(field.m_pszName);
		keyValuePair.second.offset = field.m_nSingleInheritanceOffset;
		keyValuePair.second.networked = IsFieldNetworked(field);

		keyValueMap.insert(keyValuePair);
	}

	// If this is a child class there might be a parent class with __m_pChainEntity
	if (!keyValueMap.contains(g_ChainKey) && pClassInfo->m_nBaseClassCount)
		InitChainOffset(pClassInfo->m_pBaseClasses[0].m_pClass, keyValueMap);
}

static bool InitSchemaFieldsForClass(SchemaTableMap_t& tableMap, const char* className, uint32_t classKey)
{
	CSchemaSystemTypeScope* pType = g_pSchemaSystem->FindTypeScopeForModule(MODULE_PREFIX "server" MODULE_EXT);

	if (!pType)
		return false;

	SchemaClassInfoData_t* pClassInfo = pType->FindDeclaredClass(className).Get();

	if (!pClassInfo)
	{
		SchemaKeyValueMap_t map;
		tableMap.insert(std::make_pair(classKey, map));

		Warning("InitSchemaFieldsForClass(): '%s' was not found!\n", className);
		return false;
	}

	SchemaKeyValueMap_t& keyValueMap = tableMap.insert(std::make_pair(classKey, SchemaKeyValueMap_t())).first->second;

	InitSchemaKeyValueMap(pClassInfo, keyValueMap);

	return true;
}

int16_t schema::FindChainOffset(const char* className, uint32_t classNameHash)
{
	return schema::GetOffset(className, classNameHash, "__m_pChainEntity", g_ChainKey).offset;
}

SchemaKey schema::GetOffset(const char* className, uint32_t classKey, const char* memberName, uint32_t memberKey)
{
	static SchemaTableMap_t schemaTableMap;

	if (!schemaTableMap.contains(classKey))
	{
		if (InitSchemaFieldsForClass(schemaTableMap, className, classKey))
			return GetOffset(className, classKey, memberName, memberKey);

		return {0, 0};
	}

	SchemaKeyValueMap_t tableMap = schemaTableMap[classKey];

	if (!tableMap.contains(memberKey))
	{
		if (memberKey != g_ChainKey)
			Warning("schema::GetOffset(): '%s' was not found in '%s'!\n", memberName, className);

		return {0, 0};
	}

	return tableMap[memberKey];
}

void NetworkVarStateChanged(uintptr_t pNetworkVar, uint32_t nOffset, uint32 nNetworkStateChangedOffset)
{
	NetworkStateChangedData data(nOffset);
	CALL_VIRTUAL(void, nNetworkStateChangedOffset, (void*)pNetworkVar, &data);
}

void EntityNetworkStateChanged(uintptr_t pEntity, uint nOffset)
{
	NetworkStateChangedData data(nOffset);
	reinterpret_cast<CEntityInstance*>(pEntity)->NetworkStateChanged(NetworkStateChangedData(nOffset));
}

void ChainNetworkStateChanged(uintptr_t pNetworkVarChainer, uint nLocalOffset)
{
	CEntityInstance* pEntity = reinterpret_cast<CNetworkVarChainer*>(pNetworkVarChainer)->m_pEntity;

	if (pEntity)
		pEntity->NetworkStateChanged(NetworkStateChangedData(nLocalOffset, -1, reinterpret_cast<CNetworkVarChainer*>(pNetworkVarChainer)->m_PathIndex));
}
