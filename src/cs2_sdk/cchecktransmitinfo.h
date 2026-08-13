#pragma once

#include "bitvec.h"
#include "eiface.h"
#include "playerslot.h"
#include "utlvector.h"

// https://github.com/Wend4r/sourcesdk/blob/main/public/iservernetworkable.h

struct vis_info_t_extended
{
	uint32 m_uVisBitsBufSize;
	SpawnGroupHandle_t m_SpawnGroupHandle;
	CBitVec<4096> m_VisBits;
};
COMPILE_TIME_ASSERT(sizeof(vis_info_t_extended) == 520);

class CCheckTransmitInfoExtended
{
public:
	CBitVec<MAX_EDICTS>* m_pTransmitEntity;		// entities visible/sent to client
	CBitVec<MAX_EDICTS>* m_pTransmitNonPlayers; // non-player entities needing deletion deltas
	CBitVec<MAX_EDICTS>* m_pTransmitOutOfPVS;	// entities that left PVS but still need delta update
	CBitVec<MAX_EDICTS>* m_pTransmitAlways;		// entity n is always sent even if not in PVS (HLTV and Replay only)
	CUtlVector<CPlayerSlot> m_vecTargetSlots;
	vis_info_t_extended m_VisInfo;
	CPlayerSlot m_nPlayerSlot;
	bool m_bFullUpdate = false;
};
COMPILE_TIME_ASSERT(sizeof(CCheckTransmitInfoExtended) == 584);