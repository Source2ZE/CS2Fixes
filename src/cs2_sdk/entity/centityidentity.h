#pragma once
#include <chandle.h>

class IHandleEntity;
class CEntityClass;

class CEntityIdentity
{
public:
    IHandleEntity* m_pInstance;
    CEntityClass* m_pClass;
    CHandle m_EHandle;
};