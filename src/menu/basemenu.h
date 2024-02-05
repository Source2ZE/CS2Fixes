/**
 * =============================================================================
 * CS2Fixes
 * Copyright (C) 2024 Source2ZE
 * Paging logic courtesy of CounterStrikeSharp
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
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <stack>

class ZEPlayer;
class ChatMenuInstance;

typedef std::function<void(ZEPlayer*)> MenuItemCallback;
typedef std::function<bool(ZEPlayer*)> MenuConditionHandler;

enum class MenuItemDisplayType
{
    Text,
    Spacer,
    Disabled
};

enum class MenuType
{
    Chat,
    CenterText
};

struct MenuItem
{
    std::string name;
    MenuItemCallback callback;
    MenuItemDisplayType type;
};

class BaseMenu : public std::enable_shared_from_this<BaseMenu>
{
public:
    virtual ~BaseMenu() = default;
    BaseMenu(std::string title) : m_szTitle(title) {};
    void AddItem(std::string name, MenuItemDisplayType type, MenuItemCallback callback = nullptr, ...);
    void SetCondition(MenuConditionHandler handler) { m_funcCondition = handler; }
    virtual MenuType GetMenuType() = 0;
public:
    std::string m_szTitle;
    std::vector<MenuItem> m_vecItems;
    MenuConditionHandler m_funcCondition;
    bool m_bAutoClose = true;
};

class BaseMenuInstance
{
public:
    virtual ~BaseMenuInstance() = default;
    virtual bool Render(ZEPlayer* player);
    // a menu can be destroyed without emitting OnMenuClosed (e.g. when a player disconnects)
    virtual void OnMenuClosed(ZEPlayer* player, bool intentional) {};
    void NextPage(ZEPlayer* player);
    void PrevPage(ZEPlayer* player);
    int GetOffset() const { return m_iOffset; };
    int GetPage() const { return m_iPage; };
    int GetVisibleItemCount() const { return GetNumItems() + 3 - HasPrevPage() - HasNextPage() - HasCloseButton(); };
    bool HasPrevPage() const { return m_iPage > 0; };
    bool HasNextPage() const { return m_pMenu->m_vecItems.size() > GetOffset() + GetNumItems(); };
    bool HasCloseButton() const { return true; };
    void HandleInput(ZEPlayer* player, int iInput);
    bool CheckCondition(ZEPlayer* player);
private:
    virtual int GetNumItems() const { return 6; };
public:
    std::shared_ptr<BaseMenu> m_pMenu;
private:
    int m_iPage = 0;
    int m_iOffset = 0;
    std::stack<int> m_stackOffsets;
};
