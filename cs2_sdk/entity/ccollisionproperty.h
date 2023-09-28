#pragma once

#include "../schema.h"

struct VPhysicsCollisionAttribute_t
{
	DECLARE_SCHEMA_CLASS(VPhysicsCollisionAttribute_t)

	SCHEMA_FIELD(uint8, m_nCollisionGroup)
};

class CCollisionProperty
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CCollisionProperty)

	SCHEMA_FIELD(VPhysicsCollisionAttribute_t, m_collisionAttribute)

	std::add_lvalue_reference_t<SolidType_t> m_nSolidType()
	{
		static constexpr auto datatable_hash = hash_32_fnv1a_const(ThisClass);
		static constexpr auto prop_hash = hash_32_fnv1a_const("m_nSolidType");
		static const auto m_offset = schema::GetOffset(ThisClass, datatable_hash, "m_nSolidType", prop_hash);
		return *reinterpret_cast<std::add_pointer_t<SolidType_t>>((uintptr_t)(this) + m_offset + 0);
	}

	void m_nSolidType(SolidType_t val)
	{
		static constexpr auto datatable_hash = hash_32_fnv1a_const(ThisClass);
		static constexpr auto prop_hash = hash_32_fnv1a_const("m_nSolidType");
		static const auto m_offset = schema::GetOffset(ThisClass, datatable_hash, "m_nSolidType", prop_hash);
		static const auto m_chain = schema::FindChainOffset(ThisClass);
		if (m_chain != 0)
		{
			ConMsg("Found chain offset %d for %s::%s\n", m_chain, ThisClass, "m_nSolidType");
			addresses::NetworkStateChanged((uintptr_t)(this) + m_chain, m_offset + 0, 0xFFFFFFFF);
		}
		else
		{
			ConMsg("Attempting to call update on on %s::%s\n", ThisClass, "m_nSolidType");
			vmt::CallVirtual<void>(IsStruct ? 1 : 21, this, m_offset + 0, 0xFFFFFFFF, 0xFFFF);
		}
		*reinterpret_cast<std::add_pointer_t<SolidType_t>>((uintptr_t)(this) + m_offset + 0) = val;
	}
	SCHEMA_FIELD(uint8, m_usSolidFlags)
	SCHEMA_FIELD(uint8, m_CollisionGroup)
};