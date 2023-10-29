
/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023 Source2ZE
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

#pragma once

enum class ERTVState
{
	MAP_START,
	RTV_ALLOWED,
	POST_RTV_SUCCESSFULL,
	POST_LAST_ROUND_END,
	BLOCKED_BY_ADMIN,
};

enum class EExtendState
{
	MAP_START,
	EXTEND_ALLOWED,
	POST_EXTEND_COOLDOWN,
	POST_EXTEND_NO_EXTENDS_LEFT,
	POST_LAST_ROUND_END,
	NO_EXTENDS,
};

extern ERTVState g_RTVState;
extern EExtendState g_ExtendState;
extern int g_ExtendsLeft;

void SetExtendsLeft();