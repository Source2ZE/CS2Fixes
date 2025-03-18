/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2023-2025 Source2ZE
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

extern ISteamHTTP* g_http;

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
	if (bFailed || (!m_callbackError && (arg->m_eStatusCode < 200 || arg->m_eStatusCode > 299)))
	{
		Message("HTTP request to %s failed with status code %i\n", m_strUrl.c_str(), arg->m_eStatusCode);
	}
	else
	{
		uint32 size;
		g_http->GetHTTPResponseBodySize(arg->m_hRequest, &size);

		uint8* response = new uint8[size + 1];
		g_http->GetHTTPResponseBodyData(arg->m_hRequest, response, size);
		response[size] = 0; // Add null terminator

		json jsonResponse;

		// Pass on response to the custom callback
		if (V_strcmp((char*)response, ""))
		{
			jsonResponse = json::parse((char*)response, nullptr, false);

			if (jsonResponse.is_discarded())
				Message("Failed parsing JSON from HTTP response: %s\n", (char*)response);
		}

		if (!jsonResponse.is_discarded() && (arg->m_eStatusCode < 200 || arg->m_eStatusCode > 299))
			m_callbackError(arg->m_hRequest, arg->m_eStatusCode, jsonResponse);
		else if (arg->m_eStatusCode < 200 || arg->m_eStatusCode > 299)
		{
			// Allow error callback even if invalid json, since error code can provide useful info
			m_callbackError(arg->m_hRequest, arg->m_eStatusCode, json());
		}
		else if (!jsonResponse.is_discarded())
			m_callbackCompleted(arg->m_hRequest, jsonResponse);

		delete[] response;
	}

	if (g_http)
		g_http->ReleaseHTTPRequest(arg->m_hRequest);

	delete this;
}

void HTTPManager::Get(const char* pszUrl, CompletedCallback callbackCompleted,
					  ErrorCallback callbackError, std::vector<HTTPHeader>* headers)
{
	GenerateRequest(k_EHTTPMethodGET, pszUrl, "", callbackCompleted, callbackError, headers);
}

void HTTPManager::Post(const char* pszUrl, const char* pszText, CompletedCallback callbackCompleted,
					   ErrorCallback callbackError, std::vector<HTTPHeader>* headers)
{
	GenerateRequest(k_EHTTPMethodPOST, pszUrl, pszText, callbackCompleted, callbackError, headers);
}

void HTTPManager::Put(const char* pszUrl, const char* pszText, CompletedCallback callbackCompleted,
					  ErrorCallback callbackError, std::vector<HTTPHeader>* headers)
{
	GenerateRequest(k_EHTTPMethodPUT, pszUrl, pszText, callbackCompleted, callbackError, headers);
}

void HTTPManager::Patch(const char* pszUrl, const char* pszText, CompletedCallback callbackCompleted,
						ErrorCallback callbackError, std::vector<HTTPHeader>* headers)
{
	GenerateRequest(k_EHTTPMethodPATCH, pszUrl, pszText, callbackCompleted, callbackError, headers);
}

void HTTPManager::Delete(const char* pszUrl, const char* pszText, CompletedCallback callbackCompleted,
						 ErrorCallback callbackError, std::vector<HTTPHeader>* headers)
{
	GenerateRequest(k_EHTTPMethodDELETE, pszUrl, pszText, callbackCompleted, callbackError, headers);
}

void HTTPManager::GenerateRequest(EHTTPMethod method, const char* pszUrl, const char* pszText,
								  CompletedCallback callbackCompleted, ErrorCallback callbackError,
								  std::vector<HTTPHeader>* headers)
{
	if (!g_http)
	{
		Panic("A web request was attempted before g_http was instantiated, returning early.\n");
		return;
	}

	// Message("Sending HTTP:\n%s\n", pszText);
	auto hReq = g_http->CreateHTTPRequest(method, pszUrl);
	int size = strlen(pszText);
	// Message("HTTP request: %p\n", hReq);

	bool shouldHaveBody = method == k_EHTTPMethodPOST
						  || method == k_EHTTPMethodPATCH
						  || method == k_EHTTPMethodPUT
						  || method == k_EHTTPMethodDELETE;

	if (shouldHaveBody && !g_http->SetHTTPRequestRawPostBody(hReq, "application/json", (uint8*)pszText, size))
	{
		// Message("Failed to SetHTTPRequestRawPostBody\n");
		return;
	}

	// Prevent HTTP error 411 (probably not necessary?)
	// g_http->SetHTTPRequestHeaderValue(hReq, "Content-Length", std::to_string(size).c_str());

	if (headers != nullptr)
		for (HTTPHeader header : *headers)
			g_http->SetHTTPRequestHeaderValue(hReq, header.GetName(), header.GetValue());

	SteamAPICall_t hCall;
	g_http->SendHTTPRequest(hReq, &hCall);

	new TrackedRequest(hReq, hCall, pszUrl, pszText, callbackCompleted, callbackError);
}
