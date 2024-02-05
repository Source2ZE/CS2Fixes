// Copyright (C) 2023 neverlosecc
// See end of file for extended copyright information.
/**
 * =============================================================================
 * Source2Gen
 * Copyright (C) 2023 neverlose (https://github.com/neverlosecc/source2gen)
 * =============================================================================
 **/

#pragma once
#include "../utils/virtual.h"

class CSchemaType;
class CSchemaSystemTypeScope;
class ISaveRestoreOps;

enum SchemaClassFlags_t
{
	SCHEMA_CF1_HAS_VIRTUAL_MEMBERS = 1,
	SCHEMA_CF1_IS_ABSTRACT = 2,
	SCHEMA_CF1_HAS_TRIVIAL_CONSTRUCTOR = 4,
	SCHEMA_CF1_HAS_TRIVIAL_DESTRUCTOR = 8,
	SCHEMA_CF1_HAS_NOSCHEMA_MEMBERS = 16,
	SCHEMA_CF1_IS_PARENT_CLASSES_PARSED = 32,
	SCHEMA_CF1_IS_LOCAL_TYPE_SCOPE = 64,
	SCHEMA_CF1_IS_GLOBAL_TYPE_SCOPE = 128,
	SCHEMA_CF1_IS_SCHEMA_VALIDATED = 2048,

};

enum ETypeCategory
{
	Schema_Builtin = 0,
	Schema_Ptr = 1,
	Schema_Bitfield = 2,
	Schema_FixedArray = 3,
	Schema_Atomic = 4,
	Schema_DeclaredClass = 5,
	Schema_DeclaredEnum = 6,
	Schema_None = 7
};

enum class schemafieldtype_t : uint8_t
{
	FIELD_VOID = 0x0,
	FIELD_FLOAT32 = 0x1,
	FIELD_STRING = 0x2,
	FIELD_VECTOR = 0x3,
	FIELD_QUATERNION = 0x4,
	FIELD_INT32 = 0x5,
	FIELD_BOOLEAN = 0x6,
	FIELD_INT16 = 0x7,
	FIELD_CHARACTER = 0x8,
	FIELD_COLOR32 = 0x9,
	FIELD_EMBEDDED = 0xa,
	FIELD_CUSTOM = 0xb,
	FIELD_CLASSPTR = 0xc,
	FIELD_EHANDLE = 0xd,
	FIELD_POSITION_VECTOR = 0xe,
	FIELD_TIME = 0xf,
	FIELD_TICK = 0x10,
	FIELD_SOUNDNAME = 0x11,
	FIELD_INPUT = 0x12,
	FIELD_FUNCTION = 0x13,
	FIELD_VMATRIX = 0x14,
	FIELD_VMATRIX_WORLDSPACE = 0x15,
	FIELD_MATRIX3X4_WORLDSPACE = 0x16,
	FIELD_INTERVAL = 0x17,
	FIELD_UNUSED = 0x18,
	FIELD_VECTOR2D = 0x19,
	FIELD_INT64 = 0x1a,
	FIELD_VECTOR4D = 0x1b,
	FIELD_RESOURCE = 0x1c,
	FIELD_TYPEUNKNOWN = 0x1d,
	FIELD_CSTRING = 0x1e,
	FIELD_HSCRIPT = 0x1f,
	FIELD_VARIANT = 0x20,
	FIELD_UINT64 = 0x21,
	FIELD_FLOAT64 = 0x22,
	FIELD_POSITIVEINTEGER_OR_NULL = 0x23,
	FIELD_HSCRIPT_NEW_INSTANCE = 0x24,
	FIELD_UINT32 = 0x25,
	FIELD_UTLSTRINGTOKEN = 0x26,
	FIELD_QANGLE = 0x27,
	FIELD_NETWORK_ORIGIN_CELL_QUANTIZED_VECTOR = 0x28,
	FIELD_HMATERIAL = 0x29,
	FIELD_HMODEL = 0x2a,
	FIELD_NETWORK_QUANTIZED_VECTOR = 0x2b,
	FIELD_NETWORK_QUANTIZED_FLOAT = 0x2c,
	FIELD_DIRECTION_VECTOR_WORLDSPACE = 0x2d,
	FIELD_QANGLE_WORLDSPACE = 0x2e,
	FIELD_QUATERNION_WORLDSPACE = 0x2f,
	FIELD_HSCRIPT_LIGHTBINDING = 0x30,
	FIELD_V8_VALUE = 0x31,
	FIELD_V8_OBJECT = 0x32,
	FIELD_V8_ARRAY = 0x33,
	FIELD_V8_CALLBACK_INFO = 0x34,
	FIELD_UTLSTRING = 0x35,
	FIELD_NETWORK_ORIGIN_CELL_QUANTIZED_POSITION_VECTOR = 0x36,
	FIELD_HRENDERTEXTURE = 0x37,
	FIELD_HPARTICLESYSTEMDEFINITION = 0x38,
	FIELD_UINT8 = 0x39,
	FIELD_UINT16 = 0x3a,
	FIELD_CTRANSFORM = 0x3b,
	FIELD_CTRANSFORM_WORLDSPACE = 0x3c,
	FIELD_HPOSTPROCESSING = 0x3d,
	FIELD_MATRIX3X4 = 0x3e,
	FIELD_SHIM = 0x3f,
	FIELD_CMOTIONTRANSFORM = 0x40,
	FIELD_CMOTIONTRANSFORM_WORLDSPACE = 0x41,
	FIELD_ATTACHMENT_HANDLE = 0x42,
	FIELD_AMMO_INDEX = 0x43,
	FIELD_CONDITION_ID = 0x44,
	FIELD_AI_SCHEDULE_BITS = 0x45,
	FIELD_MODIFIER_HANDLE = 0x46,
	FIELD_ROTATION_VECTOR = 0x47,
	FIELD_ROTATION_VECTOR_WORLDSPACE = 0x48,
	FIELD_HVDATA = 0x49,
	FIELD_SCALE32 = 0x4a,
	FIELD_STRING_AND_TOKEN = 0x4b,
	FIELD_ENGINE_TIME = 0x4c,
	FIELD_ENGINE_TICK = 0x4d,
	FIELD_WORLD_GROUP_ID = 0x4e,
	FIELD_TYPECOUNT = 0x4f,
};

struct CSchemaVarName
{
	const char *m_name;
	const char *m_type;
};

struct CSchemaNetworkValue
{
	union
	{
		const char *m_p_sz_value;
		int m_n_value;
		float m_f_value;
		uintptr_t m_p_value;
		CSchemaVarName m_var_value;
		char m_sz_value[32];
	};
};

struct SchemaMetadataEntryData_t
{
	const char *m_name;
	CSchemaNetworkValue *m_value;
};

struct SchemaFieldMetadataOverrideData_t
{
	schemafieldtype_t m_field_type;			 // 0x0000
	char pad_0001[7];						 // 0x0001
	const char *m_field_name;				 // 0x0008
	uint32_t m_single_inheritance_offset;	 // 0x0010
	int32_t m_field_count;					 // 0x0014 // @note: @og: if its array or smth like this it will point to count of array
	int32_t m_i_unk_1;						 // 0x0018
	char pad_001C[12];						 // 0x001C
	ISaveRestoreOps *m_def_save_restore_ops; // 0x0028
	char pad_0030[16];						 // 0x0030
	uint32_t m_align;						 // 0x0040
	char pad_0044[36];						 // 0x0044
};											 // Size: 0x0068

struct SchemaFieldMetadataOverrideSetData_t
{
	SchemaFieldMetadataOverrideData_t *m_metadata_override_data; // 0x0008
	int32_t m_size;												 // 0x0008
};

struct SchemaStaticFieldData_t
{
	const char *name;	   // 0x0000
	CSchemaType *m_type;   // 0x0008
	void *m_instance;	   // 0x0010
	char pad_0x0018[0x10]; // 0x0018
};

struct SchemaClassFieldData_t
{
	const char *m_name;					   // 0x0000
	void *m_type;						   // 0x0008
	int32 m_single_inheritance_offset;	   // 0x0010
	int32 m_metadata_size;				   // 0x0014
	SchemaMetadataEntryData_t *m_metadata; // 0x0018
};

class SchemaEnumInfoData_t
{
public:
	SchemaEnumInfoData_t *m_self;		 // 0x0000
	const char *m_name;					 // 0x0008
	const char *m_module;				 // 0x0010
	int8_t m_align;				 // 0x0018
	char pad_0x0019[0x3];				 // 0x0019
	int16_t m_size;				 // 0x001C
	int16_t m_static_metadata_size; // 0x001E
	void *m_enum_info;
	SchemaMetadataEntryData_t *m_static_metadata;
	CSchemaSystemTypeScope *m_type_scope; // 0x0030
	char pad_0x0038[0x8];				  // 0x0038
	int32_t m_i_unk1;				  // 0x0040
};

class SchemaClassInfoData_t;

struct SchemaBaseClassInfoData_t
{
	unsigned int m_offset;
	SchemaClassInfoData_t *m_class;
};

class SchemaClassInfoData_t
{
public:
	auto GetName()
	{
		return m_name;
	}

	auto GetFieldsSize()
	{
		return m_fields_size;
	}

	auto GetFields()
	{
		return m_fields;
	}

	SchemaClassInfoData_t *GetParent()
	{
		if (!m_base_classes)
			return nullptr;

		return m_base_classes->m_class;
	}

private:
	SchemaClassInfoData_t *m_self;									  // 0x0000
	const char *m_name;												  // 0x0008
	const char *m_module;											  // 0x0010

	int m_size;														  // 0x0018
	int16_t m_fields_size;											  // 0x001C
	int16_t m_static_fields_size;									  // 0x001E
	int16_t m_static_metadata_size;									  // 0x0020
	uint8_t m_align_of;												  // 0x0022
	uint8_t m_has_base_class;										  // 0x0023
	int16_t m_total_class_size;										  // 0x0024 // @note: @og: if there no derived or base class then it will be 1 otherwise derived class size + 1.
	int16_t m_derived_class_size;									  // 0x0026
	SchemaClassFieldData_t *m_fields;								  // 0x0028
	SchemaStaticFieldData_t *m_static_fields;						  // 0x0030
	SchemaBaseClassInfoData_t *m_base_classes;						  // 0x0038
	SchemaFieldMetadataOverrideSetData_t *m_field_metadata_overrides; // 0x0040
	SchemaMetadataEntryData_t *m_static_metadata;					  // 0x0048
	CSchemaSystemTypeScope *m_type_scope;							  // 0x0050
	CSchemaType *m_schema_type;										  // 0x0058
	SchemaClassFlags_t m_class_flags : 8;							  // 0x0060
	uint32_t m_sequence;											  // 0x0064 // @note: @og: idk
	void *m_fn;														  // 0x0068
};

class CSchemaType
{
public:
	bool GetSizes(int *out_size1, uint8_t *unk_probably_not_size)
	{
		return reinterpret_cast<int(THISCALL *)(void *, int *, uint8_t *)>(vftable_[3])(this, out_size1, unk_probably_not_size);
	}

public:
	bool GetSize(int *out_size)
	{
		uint8_t smh = 0;
		return GetSizes(out_size, &smh);
	}

public:
	uintptr_t *vftable_;			   // 0x0000
	const char *m_name_;				   // 0x0008

	CSchemaSystemTypeScope *m_type_scope_; // 0x0010
	uint8_t type_category;			   // ETypeCategory 0x0018
	uint8_t atomic_category;		   // EAtomicCategory 0x0019

										   // find out to what class pointer points.
	CSchemaType *GetRefClass() const
	{
		if (type_category != Schema_Ptr)
			return nullptr;

		auto ptr = m_schema_type_;
		while (ptr && ptr->type_category == ETypeCategory::Schema_Ptr)
			ptr = ptr->m_schema_type_;

		return ptr;
	}

	struct array_t
	{
		uint32_t array_size;
		uint32_t unknown;
		CSchemaType *element_type_;
	};

	struct atomic_t
	{ // same goes for CollectionOfT
		uint64_t gap[2];
		CSchemaType *template_typename;
	};

	struct atomic_tt
	{
		uint64_t gap[2];
		CSchemaType *templates[2];
	};

	struct atomic_i
	{
		uint64_t gap[2];
		uint64_t integer;
	};

	// this union depends on CSchema implementation, all members above
	// is from base class ( CSchemaType )
	union // 0x020
	{
		CSchemaType *m_schema_type_;
		SchemaClassInfoData_t *m_class_info;
		SchemaEnumInfoData_t *m_enum_binding_;
		array_t m_array_;
		atomic_t m_atomic_t_;
		atomic_tt m_atomic_tt_;
		atomic_i m_atomic_i_;
	};
};

class CSchemaSystemTypeScope
{
public:
	SchemaClassInfoData_t *FindDeclaredClass(const char *pClass)
	{
#ifdef _WIN32
		SchemaClassInfoData_t *rv = nullptr;
		CALL_VIRTUAL(void, 2, this, &rv, pClass);
		return rv;
#else
		return CALL_VIRTUAL(SchemaClassInfoData_t*, 2, this, pClass);
#endif
	}
};

class CSchemaSystem
{
public:
	auto FindTypeScopeForModule(const char *module)
	{
		return CALL_VIRTUAL(CSchemaSystemTypeScope *, 13, this, module, nullptr);
	}
};

// source2gen - Source2 games SDK generator
// Copyright 2023 neverlosecc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
