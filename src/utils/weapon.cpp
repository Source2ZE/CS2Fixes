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

#include "weapon.h"

#include <ranges>

static std::unordered_map<std::string, WeaponInfo_t> s_WeaponMap = {
	{"weapon_deagle",				  {"weapon_deagle", 1, 0, GEAR_SLOT_PISTOL, 700, "Desert Eagle", {"deagle"}}						},
	{"weapon_elite",				 {"weapon_elite", 2, 0, GEAR_SLOT_PISTOL, 300, "Dual Berettas", {"dualberettas", "elite"}}		  },
	{"weapon_fiveseven",			 {"weapon_fiveseven", 3, 3, GEAR_SLOT_PISTOL, 500, "Five-SeveN", {"fiveseven"}}					   },
	{"weapon_glock",				 {"weapon_glock", 4, 2, GEAR_SLOT_PISTOL, 200, "Glock-18", {"glock18", "glock"}}					},
	{"weapon_ak47",					{"weapon_ak47", 7, 2, GEAR_SLOT_RIFLE, 2700, "AK-47", {"ak47", "ak"}}							 },
	{"weapon_aug",				   {"weapon_aug", 8, 3, GEAR_SLOT_RIFLE, 3300, "AUG", {"aug"}}										  },
	{"weapon_awp",				   {"weapon_awp", 9, 0, GEAR_SLOT_RIFLE, 4750, "AWP", {"awp"}}										  },
	{"weapon_famas",				 {"weapon_famas", 10, 3, GEAR_SLOT_RIFLE, 2050, "Famas", {"famas"}}								   },
	{"weapon_g3sg1",				 {"weapon_g3sg1", 11, 2, GEAR_SLOT_RIFLE, 5000, "G3SG1", {"g3sg1"}}								   },
	{"weapon_galilar",			   {"weapon_galilar", 13, 2, GEAR_SLOT_RIFLE, 1800, "Galil AR", {"galilar", "galil"}}				 },
	{"weapon_m249",					{"weapon_m249", 14, 0, GEAR_SLOT_RIFLE, 5200, "M249", {"m249"}}								   },
	{"weapon_m4a1",					{"weapon_m4a1", 16, 3, GEAR_SLOT_RIFLE, 3100, "M4A4", {"m4a4"}}								   },
	{"weapon_mac10",				 {"weapon_mac10", 17, 2, GEAR_SLOT_RIFLE, 1050, "MAC-10", {"mac10"}}								},
	{"weapon_p90",				   {"weapon_p90", 19, 0, GEAR_SLOT_RIFLE, 2350, "P90", {"p90"}}									   },
	{"weapon_mp5sd",				 {"weapon_mp5sd", 23, 0, GEAR_SLOT_RIFLE, 1500, "MP5-SD", {"mp5sd", "mp5"}}						   },
	{"weapon_ump45",				 {"weapon_ump45", 24, 0, GEAR_SLOT_RIFLE, 1200, "UMP-45", {"ump45", "ump"}}						   },
	{"weapon_xm1014",				  {"weapon_xm1014", 25, 0, GEAR_SLOT_RIFLE, 2000, "XM1014", {"xm1014", "xm"}}						 },
	{"weapon_bizon",				 {"weapon_bizon", 26, 0, GEAR_SLOT_RIFLE, 1400, "PP-Bizon", {"bizon"}}							  },
	{"weapon_mag7",					{"weapon_mag7", 27, 3, GEAR_SLOT_RIFLE, 1300, "MAG-7", {"mag7", "mag"}}						   },
	{"weapon_negev",				 {"weapon_negev", 28, 0, GEAR_SLOT_RIFLE, 1700, "Negev", {"negev"}}								   },
	{"weapon_sawedoff",				{"weapon_sawedoff", 29, 2, GEAR_SLOT_RIFLE, 1100, "Sawed-Off", {"sawedoff"}}						},
	{"weapon_tec9",					{"weapon_tec9", 30, 2, GEAR_SLOT_RIFLE, 500, "Tec-9", {"tec9"}}								   },
	{"weapon_hkp2000",			   {"weapon_hkp2000", 32, 3, GEAR_SLOT_PISTOL, 200, "P2000", {"p2000"}}							   },
	{"weapon_mp7",				   {"weapon_mp7", 33, 0, GEAR_SLOT_RIFLE, 1500, "MP7", {"mp7"}}									   },
	{"weapon_mp9",				   {"weapon_mp9", 34, 0, GEAR_SLOT_RIFLE, 1250, "MP9", {"mp9"}}									   },
	{"weapon_nova",					{"weapon_nova", 35, 0, GEAR_SLOT_RIFLE, 1050, "Nova", {"nova"}}								   },
	{"weapon_p250",					{"weapon_p250", 36, 0, GEAR_SLOT_PISTOL, 300, "P250", {"p250"}}								   },
	{"weapon_scar20",				  {"weapon_scar20", 38, 3, GEAR_SLOT_RIFLE, 5000, "SCAR-20", {"scar20", "scar"}}					},
	{"weapon_sg556",				 {"weapon_sg556", 39, 2, GEAR_SLOT_RIFLE, 3000, "SG 553", {"sg553"}}								},
	{"weapon_ssg08",				 {"weapon_ssg08", 40, 0, GEAR_SLOT_RIFLE, 1700, "SSG 08", {"ssg08", "ssg"}}						   },
	{"weapon_m4a1_silencer",		 {"weapon_m4a1_silencer", 60, 3, GEAR_SLOT_RIFLE, 2900, "M4A1-S", {"m4a1-s", "m4a1"}}				 },
	{"weapon_usp_silencer",			{"weapon_usp_silencer", 61, 3, GEAR_SLOT_PISTOL, 200, "USP-S", {"usp-s", "usp"}}					},
	{"weapon_cz75a",				 {"weapon_cz75a", 63, 0, GEAR_SLOT_PISTOL, 500, "CZ75-Auto", {"cz75-auto", "cs75a", "cz"}}		  },
	{"weapon_revolver",				{"weapon_revolver", 64, 0, GEAR_SLOT_PISTOL, 600, "R8 Revolver", {"r8revolver", "revolver", "r8"}}},

	{"weapon_flashbang",			 {"weapon_flashbang", 43, 0, GEAR_SLOT_GRENADES}													},
	{"weapon_hegrenade",			 {"weapon_hegrenade", 44, 0, GEAR_SLOT_GRENADES, 300, "HE Grenade", {"hegrenade", "he"}, 1}		   },
	{"weapon_smokegrenade",			{"weapon_smokegrenade", 45, 0, GEAR_SLOT_GRENADES}												  },
	{"weapon_molotov",			   {"weapon_molotov", 46, 0, GEAR_SLOT_GRENADES, 400, "Molotov", {"molotov"}, 1}						},
	{"weapon_decoy",				 {"weapon_decoy", 47, 0, GEAR_SLOT_GRENADES}														},
	{"weapon_incgrenade",			  {"weapon_incgrenade", 48, 0, GEAR_SLOT_GRENADES}												  },

	{"weapon_taser",				 {"weapon_taser", 31, 0, GEAR_SLOT_KNIFE}															 },
	{"weapon_knifegg",			   {"weapon_knifegg", 41, 0, GEAR_SLOT_KNIFE}														 },
	{"weapon_knife",				 {"weapon_knife", 42, 0, GEAR_SLOT_KNIFE}															 },
	{"weapon_c4",					{"weapon_c4", 49, 0, GEAR_SLOT_C4}																},
	{"weapon_knife_t",			   {"weapon_knife", 59, 0, GEAR_SLOT_KNIFE}														   },
	{"weapon_bayonet",			   {"weapon_knife", 500, 0, GEAR_SLOT_KNIFE}															},
	{"weapon_knife_css",			 {"weapon_knife", 503, 0, GEAR_SLOT_KNIFE}														  },
	{"weapon_knife_flip",			  {"weapon_knife", 505, 0, GEAR_SLOT_KNIFE}														   },
	{"weapon_knife_gut",			 {"weapon_knife", 506, 0, GEAR_SLOT_KNIFE}														  },
	{"weapon_knife_karambit",		  {"weapon_knife", 507, 0, GEAR_SLOT_KNIFE}														   },
	{"weapon_knife_m9_bayonet",		{"weapon_knife", 508, 0, GEAR_SLOT_KNIFE}														 },
	{"weapon_knife_tactical",		  {"weapon_knife", 509, 0, GEAR_SLOT_KNIFE}														   },
	{"weapon_knife_falchion",		  {"weapon_knife", 512, 0, GEAR_SLOT_KNIFE}														   },
	{"weapon_knife_survival_bowie",	{"weapon_knife", 514, 0, GEAR_SLOT_KNIFE}														 },
	{"weapon_knife_butterfly",	   {"weapon_knife", 515, 0, GEAR_SLOT_KNIFE}															},
	{"weapon_knife_push",			  {"weapon_knife", 516, 0, GEAR_SLOT_KNIFE}														   },
	{"weapon_knife_cord",			  {"weapon_knife", 517, 0, GEAR_SLOT_KNIFE}														   },
	{"weapon_knife_canis",		   {"weapon_knife", 518, 0, GEAR_SLOT_KNIFE}															},
	{"weapon_knife_ursus",		   {"weapon_knife", 519, 0, GEAR_SLOT_KNIFE}															},
	{"weapon_knife_gypsy_jackknife", {"weapon_knife", 520, 0, GEAR_SLOT_KNIFE}														  },
	{"weapon_knife_outdoor",		 {"weapon_knife", 521, 0, GEAR_SLOT_KNIFE}														  },
	{"weapon_knife_stiletto",		  {"weapon_knife", 522, 0, GEAR_SLOT_KNIFE}														   },
	{"weapon_knife_widowmaker",		{"weapon_knife", 523, 0, GEAR_SLOT_KNIFE}														 },
	{"weapon_knife_skeleton",		  {"weapon_knife", 525, 0, GEAR_SLOT_KNIFE}														   },
	{"weapon_knife_kukri",		   {"weapon_knife", 526, 0, GEAR_SLOT_KNIFE}															},

	{"item_kevlar",					{"item_kevlar", 0, 0, GEAR_SLOT_UTILITY, 650, "Kevlar Vest", {"kevlar"}}							},
	{"item_assaultsuit",			 {"item_assaultsuit", 0, 0, GEAR_SLOT_UTILITY}													  },
	{"item_heavyassaultsuit",		  {"item_heavyassaultsuit", 0, 0, GEAR_SLOT_UTILITY}												},
	{"item_defuser",				 {"item_defuser", 0, 0, GEAR_SLOT_UTILITY}														  },
	{"ammo_50ae",					{"ammo_50ae", 0, 0, GEAR_SLOT_UTILITY}															},
};

const WeaponInfo_t* FindWeaponInfoByClass(const char* pClass)
{
	const auto& find = s_WeaponMap.find(pClass);
	return find == s_WeaponMap.end() ? nullptr : &find->second;
}

const WeaponInfo_t* FindWeaponInfoByClassCaseInsensitive(const char* pClass)
{
	CUtlString string(pClass);
	string.ToLowerFast();
	const auto& find = s_WeaponMap.find(string.Get());
	return find == s_WeaponMap.end() ? nullptr : &find->second;
}

const WeaponInfo_t* FindWeaponInfoByAlias(const char* pAlias)
{
	for (const auto& info : s_WeaponMap | std::views::values)
	{
		if (info.m_vecAliases.empty())
			continue;

		for (const auto& alias : info.m_vecAliases)
			if (V_stricmp(pAlias, alias.c_str()) == 0)
				return &info;
	}

	return nullptr;
}

std::vector<std::pair<std::string, std::vector<std::string>>> GenerateWeaponCommands()
{
	std::vector<std::pair<std::string, std::vector<std::string>>> vec;

	for (const auto& weapon : s_WeaponMap)
	{
		if (weapon.second.m_nPrice <= 0)
			continue;

		const auto& classname = weapon.first;
		const auto& aliases = weapon.second.m_vecAliases;

		vec.emplace_back(classname, aliases);
	}

	return vec;
}