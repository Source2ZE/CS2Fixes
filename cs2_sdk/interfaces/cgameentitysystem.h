#pragma once
#include "entity/cbaseentity.h"
#include "platform.h"

#define MAX_ENTITIES_IN_LIST 512
#define MAX_ENTITY_LISTS	 64
#define MAX_TOTAL_ENTITIES	 MAX_ENTITIES_IN_LIST *MAX_ENTITY_LISTS

class CEntityIdentity
{
public:
	CBaseEntity *entity;
	void *dunno;
	int64 unk0;
	int64 unk1;
	const char *internalName;
	const char *entityName;
	void *unk2;
	void *unk3;
	void *unk4;
	void *unk5;
	CEntityIdentity *prevValid;
	CEntityIdentity *nextValid;
	void *unkptr;
	void *unkptr2;
	void *unkptr3;
};

class CEntityIdentities
{
public:
	CEntityIdentity m_pIdentities[MAX_ENTITIES_IN_LIST];
};

class EntityIdentityList
{
public:
	CEntityIdentities *m_pIdentityList;
};

class CGameEntitySystem
{
public:
	virtual void n_0();
	void *unk;
	CEntityIdentities *m_pEntityList[MAX_ENTITY_LISTS];

	CBaseEntity *GetBaseEntity(int index)
	{
		if (index <= -1 || index >= MAX_TOTAL_ENTITIES)
			return nullptr;

		int listToUse = (index / MAX_ENTITIES_IN_LIST);
		if (!m_pEntityList[listToUse])
			return nullptr;

		return m_pEntityList[listToUse]->m_pIdentities[index % MAX_ENTITIES_IN_LIST].entity;
	}

	static CGameEntitySystem *GetInstance();
};