/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2024 Source2ZE
 * Original code from D2Lobby2
 * Copyright (C) 2023 Nicholas Hastings
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 2.0 or later, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, you are also granted permission to link the code
 * of this program (as well as its derivative works) to "Dota 2," the
 * "Source Engine, and any Game MODs that run on software by the Valve Corporation.
 * You must obey the GNU General Public License in all respects for all other
 * code used.  Additionally, this exception is granted to all derivative works.
 */

#pragma once

#include "cs2fixes.h"
#undef snprintf
#include "vendor/nlohmann/json_fwd.hpp"
#include <steam/steam_gameserver.h>

#include <functional>
#include <vector>

using json = nlohmann::json;

class HTTPManager;
extern HTTPManager g_HTTPManager;

#define CompletedCallback std::function<void(HTTPRequestHandle, json)>

class HTTPHeader
{
public:
	HTTPHeader(std::string strName, std::string strValue)
	{
		m_strName = strName;
		m_strValue = strValue;
	}
	const char* GetName() { return m_strName.c_str(); }
	const char* GetValue() { return m_strValue.c_str(); }

private:
	std::string m_strName;
	std::string m_strValue;
};

class HTTPManager
{
public:
	void GET(const char* pszUrl, CompletedCallback callback, std::vector<HTTPHeader>* headers = nullptr);
	void POST(const char* pszUrl, const char* pszText, CompletedCallback callback, std::vector<HTTPHeader>* headers = nullptr);
	bool HasAnyPendingRequests() const { return m_PendingRequests.size() > 0; }

private:
	class TrackedRequest
	{
	public:
		TrackedRequest(const TrackedRequest& req) = delete;
		TrackedRequest(HTTPRequestHandle hndl, SteamAPICall_t hCall, std::string strUrl, std::string strText, CompletedCallback callback);
		~TrackedRequest();

	private:
		void OnHTTPRequestCompleted(HTTPRequestCompleted_t* arg, bool bFailed);

		HTTPRequestHandle m_hHTTPReq;
		CCallResult<TrackedRequest, HTTPRequestCompleted_t> m_CallResult;
		std::string m_strUrl;
		std::string m_strText;
		CompletedCallback m_callback;
	};

private:
	std::vector<HTTPManager::TrackedRequest*> m_PendingRequests;
	void GenerateRequest(EHTTPMethod method, const char* pszUrl, const char* pszText, CompletedCallback callback, std::vector<HTTPHeader>* headers);
};