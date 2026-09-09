/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2026 Source2ZE
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

#include "httpmanager.h"
#include "common.h"
#include "vendor/nlohmann/json.hpp"
#include <string>

HTTPManager g_HTTPManager;

#undef strdup

HTTPManager::TrackedRequest::TrackedRequest(HTTPRequestHandle hndl, SteamAPICall_t hCall,
											std::string strUrl, std::string strText,
											CompletedCallback callbackCompleted, ErrorCallback callbackError)
{
	m_hHTTPReq = hndl;
	m_CallResult.SetGameserverFlag();
	m_CallResult.Set(hCall, this, &TrackedRequest::OnHTTPRequestCompleted);

	m_strUrl = strUrl;
	m_strText = strText;
	m_callbackCompleted = callbackCompleted;
	m_callbackError = callbackError;

	g_HTTPManager.m_PendingRequests.push_back(this);
}

HTTPManager::TrackedRequest::~TrackedRequest()
{
	for (auto e = g_HTTPManager.m_PendingRequests.begin(); e != g_HTTPManager.m_PendingRequests.end(); ++e)
	{
		if (*e == this)
		{
			g_HTTPManager.m_PendingRequests.erase(e);
			break;
		}
	}
}

void HTTPManager::TrackedRequest::OnHTTPRequestCompleted(HTTPRequestCompleted_t* arg, bool bFailed)
{
	bool bTransportError = bFailed || !arg->m_bRequestSuccessful;
	bool bHTTPError = arg->m_eStatusCode < 200 || arg->m_eStatusCode > 299;

	if (bTransportError && m_callbackError)
	{
		m_callbackError(arg->m_hRequest, k_EHTTPStatusCodeInvalid, json());
	}
	else if (bTransportError || (bHTTPError && !m_callbackError))
	{
		Message("HTTP request to %s failed with status code %i\n", m_strUrl.c_str(), arg->m_eStatusCode);
	}
	else
	{
		uint32 size;
		GetSteamHTTP()->GetHTTPResponseBodySize(arg->m_hRequest, &size);

		uint8* response = new uint8[size + 1];
		GetSteamHTTP()->GetHTTPResponseBodyData(arg->m_hRequest, response, size);
		response[size] = 0; // Add null terminator

		json jsonResponse;

		// Pass on response to the custom callback
		if (V_strcmp((char*)response, ""))
		{
			jsonResponse = json::parse((char*)response, nullptr, false);

			if (jsonResponse.is_discarded())
				Message("Failed parsing JSON from HTTP response: %s\n", (char*)response);
		}

		if (!jsonResponse.is_discarded() && bHTTPError)
			m_callbackError(arg->m_hRequest, arg->m_eStatusCode, jsonResponse);
		else if (bHTTPError)
		{
			// Allow error callback even if invalid json, since error code can provide useful info
			m_callbackError(arg->m_hRequest, arg->m_eStatusCode, json());
		}
		else if (!jsonResponse.is_discarded() && m_callbackCompleted)
			m_callbackCompleted(arg->m_hRequest, jsonResponse);

		delete[] response;
	}

	if (GetSteamHTTP())
		GetSteamHTTP()->ReleaseHTTPRequest(arg->m_hRequest);

	delete this;
}

void HTTPManager::Get(std::string strUrl, CompletedCallback callbackCompleted,
					  ErrorCallback callbackError, std::vector<HTTPHeader>* headers,
					  int absoluteTimeoutMs)
{
	GenerateRequest(k_EHTTPMethodGET, strUrl, "", callbackCompleted, callbackError, headers, absoluteTimeoutMs);
}

void HTTPManager::Post(std::string strUrl, std::string strText, CompletedCallback callbackCompleted,
					   ErrorCallback callbackError, std::vector<HTTPHeader>* headers,
					   int absoluteTimeoutMs)
{
	GenerateRequest(k_EHTTPMethodPOST, strUrl, strText, callbackCompleted, callbackError, headers, absoluteTimeoutMs);
}

void HTTPManager::Put(std::string strUrl, std::string strText, CompletedCallback callbackCompleted,
					  ErrorCallback callbackError, std::vector<HTTPHeader>* headers,
					  int absoluteTimeoutMs)
{
	GenerateRequest(k_EHTTPMethodPUT, strUrl, strText, callbackCompleted, callbackError, headers, absoluteTimeoutMs);
}

void HTTPManager::Patch(std::string strUrl, std::string strText, CompletedCallback callbackCompleted,
						ErrorCallback callbackError, std::vector<HTTPHeader>* headers, int absoluteTimeoutMs)
{
	GenerateRequest(k_EHTTPMethodPATCH, strUrl, strText, callbackCompleted, callbackError, headers, absoluteTimeoutMs);
}

void HTTPManager::Delete(std::string strUrl, std::string strText, CompletedCallback callbackCompleted,
						 ErrorCallback callbackError, std::vector<HTTPHeader>* headers, int absoluteTimeoutMs)
{
	GenerateRequest(k_EHTTPMethodDELETE, strUrl, strText, callbackCompleted, callbackError, headers, absoluteTimeoutMs);
}

void HTTPManager::GenerateRequest(EHTTPMethod method, std::string strUrl, std::string strText,
								  CompletedCallback callbackCompleted, ErrorCallback callbackError,
								  std::vector<HTTPHeader>* headers, int absoluteTimeoutMs)
{
	if (!GetSteamHTTP())
	{
		Panic("A web request for %s was attempted on null ISteamHTTP, returning early.\n", strUrl.c_str());
		return;
	}

#ifdef _DEBUG
	Message("Sending HTTP:\n%s\n", strText.c_str());
#endif

	HTTPRequestHandle hReq = GetSteamHTTP()->CreateHTTPRequest(method, strUrl.c_str());
	if (hReq == INVALID_HTTPREQUEST_HANDLE)
	{
		Panic("Failed to CreateHTTPRequest for %s\n", strUrl.c_str());
		return;
	}

	bool shouldHaveBody = method == k_EHTTPMethodPOST
						  || method == k_EHTTPMethodPATCH
						  || method == k_EHTTPMethodPUT
						  || method == k_EHTTPMethodDELETE;

	if (shouldHaveBody && !GetSteamHTTP()->SetHTTPRequestRawPostBody(hReq, "application/json", (uint8*)(strText.c_str()), strText.length()))
	{
		Panic("Failed to SetHTTPRequestRawPostBody for %s\n", strUrl.c_str());
		GetSteamHTTP()->ReleaseHTTPRequest(hReq);
		return;
	}

	if (headers != nullptr)
	{
		for (HTTPHeader header : *headers)
		{
			if (!GetSteamHTTP()->SetHTTPRequestHeaderValue(hReq, header.GetName(), header.GetValue()))
			{
				Panic("Failed to SetHTTPRequestHeaderValue for %s\n", strUrl.c_str());
				GetSteamHTTP()->ReleaseHTTPRequest(hReq);
				return;
			}
		}
	}

	if (absoluteTimeoutMs != 0 && !GetSteamHTTP()->SetHTTPRequestAbsoluteTimeoutMS(hReq, static_cast<uint32_t>(absoluteTimeoutMs)))
	{
		Panic("Failed to SetHTTPRequestAbsoluteTimeoutMS for %s\n", strUrl.c_str());
		GetSteamHTTP()->ReleaseHTTPRequest(hReq);
		return;
	}

	SteamAPICall_t hCall;
	if (!GetSteamHTTP()->SendHTTPRequest(hReq, &hCall))
	{
		Panic("Failed to SendHTTPRequest for %s\n", strUrl.c_str());
		GetSteamHTTP()->ReleaseHTTPRequest(hReq);
		return;
	}

	new TrackedRequest(hReq, hCall, strUrl, strText, callbackCompleted, callbackError);
}
