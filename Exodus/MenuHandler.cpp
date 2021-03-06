#include "MenuHandler.h"
#include "AboutViewPresenter.h"
#include "SettingsViewPresenter.h"
#include "ModuleManagerViewPresenter.h"
#include "CreateDashboardViewPresenter.h"

//----------------------------------------------------------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------------------------------------------------------
MenuHandler::MenuHandler(ExodusInterface& owner, ExodusInterface& model)
:MenuHandlerBase(L"GUIMenus", owner.GetViewManager()), _owner(owner), _model(model)
{ }

//----------------------------------------------------------------------------------------------------------------------
// Management functions
//----------------------------------------------------------------------------------------------------------------------
void MenuHandler::GetMenuItems(std::list<MenuItemDefinition>& menuItems) const
{
	menuItems.push_back(MenuItemDefinition(MENUITEM_ABOUT, L"About", AboutViewPresenter::GetUnqualifiedViewTitle(), true));
	menuItems.push_back(MenuItemDefinition(MENUITEM_SETTINGS, L"Settings", SettingsViewPresenter::GetUnqualifiedViewTitle(), true));
	menuItems.push_back(MenuItemDefinition(MENUITEM_MODULEMANAGER, L"ModuleManager", ModuleManagerViewPresenter::GetUnqualifiedViewTitle(), true));
	menuItems.push_back(MenuItemDefinition(MENUITEM_CREATEDASHBOARD, L"CreateDashboard", CreateDashboardViewPresenter::GetUnqualifiedViewTitle(), true));
}

//----------------------------------------------------------------------------------------------------------------------
IViewPresenter* MenuHandler::CreateViewForItem(int menuItemID, const std::wstring& viewName)
{
	switch (menuItemID)
	{
	case MENUITEM_ABOUT:
		return new AboutViewPresenter(GetMenuHandlerName(), viewName, menuItemID, _owner, _model);
	case MENUITEM_SETTINGS:
		return new SettingsViewPresenter(GetMenuHandlerName(), viewName, menuItemID, _owner, _model);
	case MENUITEM_MODULEMANAGER:
		return new ModuleManagerViewPresenter(GetMenuHandlerName(), viewName, menuItemID, _owner, _model);
	case MENUITEM_CREATEDASHBOARD:
		return new CreateDashboardViewPresenter(GetMenuHandlerName(), viewName, menuItemID, _owner, _model);
	}
	return 0;
}

//----------------------------------------------------------------------------------------------------------------------
void MenuHandler::DeleteViewForItem(int menuItemID, IViewPresenter* viewPresenter)
{
	delete viewPresenter;
}
