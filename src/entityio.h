#pragma once

#include <string_t.h>
#include <entityhandle.h>

enum EntityIOTargetType_t
{
	ENTITY_IO_TARGET_INVALID = 0xFFFFFFFF,
	ENTITY_IO_TARGET_CLASSNAME = 0x0,
	ENTITY_IO_TARGET_CLASSNAME_DERIVES_FROM = 0x1,
	ENTITY_IO_TARGET_ENTITYNAME = 0x2,
	ENTITY_IO_TARGET_CONTAINS_COMPONENT = 0x3,
	ENTITY_IO_TARGET_SPECIAL_ACTIVATOR = 0x4,
	ENTITY_IO_TARGET_SPECIAL_CALLER = 0x5,
	ENTITY_IO_TARGET_EHANDLE = 0x6,
	ENTITY_IO_TARGET_ENTITYNAME_OR_CLASSNAME = 0x7,
};

struct EntityIOConnectionDesc_t
{
	string_t m_targetDesc;
	string_t m_targetInput;
	string_t m_valueOverride;
	CEntityHandle m_hTarget;
	EntityIOTargetType_t m_nTargetType;
	int32 m_nTimesToFire;
	float m_flDelay;
};

struct EntityIOConnection_t : EntityIOConnectionDesc_t
{
	bool m_bMarkedForRemoval;
	EntityIOConnection_t* m_pNext;
};

struct EntityIOOutputDesc_t
{
	const char* m_pName;
	uint32 m_nFlags;
	uint32 m_nOutputOffset;
};

class CEntityIOOutput
{
public:
	void* vtable;
	EntityIOConnection_t* m_pConnections;
	EntityIOOutputDesc_t* m_pDesc;
};