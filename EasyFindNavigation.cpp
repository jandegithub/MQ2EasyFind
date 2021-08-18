
#include "EasyFind.h"
#include "EasyFindConfiguration.h"

#include "MQ2Nav/PluginAPI.h"

static nav::NavAPI* s_nav = nullptr;
static int s_navObserverId = 0;

FindLocationRequestState g_activeFindState;

//----------------------------------------------------------------------------

bool Navigation_ExecuteCommand(FindLocationRequestState&& request)
{
	if (!s_nav)
		return false;

	g_activeFindState = std::move(request);

	char command[256] = { 0 };

	if (g_activeFindState.type == FindLocation_Player)
	{
		// nav by spawnID:
		sprintf_s(command, "spawn id %d | dist=15 log=warning tag=easyfind", g_activeFindState.spawnID);
	}
	else if (g_activeFindState.type == FindLocation_Switch || g_activeFindState.type == FindLocation_Location)
	{
		if (g_activeFindState.findableLocation)
		{
			if (!g_activeFindState.findableLocation->spawnName.empty())
			{
				sprintf_s(command, "spawn %s | dist=15 log=warning tag=easyfind", g_activeFindState.findableLocation->spawnName.c_str());
			}
		}

		if (command[0] == 0)
		{
			if (g_activeFindState.location != glm::vec3())
			{
				glm::vec3 loc = g_activeFindState.location;
				loc.z = pDisplay->GetFloorHeight(loc.x, loc.y, loc.z, 2.0f);
				sprintf_s(command, "locyxz %.2f %.2f %.2f log=warning tag=easyfind", loc.x, loc.y, loc.z);

				if (g_activeFindState.type == FindLocation_Switch)
					g_activeFindState.activateSwitch = true;
			}
			else if (g_activeFindState.type == FindLocation_Switch)
			{
				sprintf_s(command, "door id %d click log=warning tag=easyfind", g_activeFindState.switchID);
			}
		}
	}

	if (command[0] != 0)
	{
		s_nav->ExecuteNavCommand(command);
		return true;
	}

	return false;
}

void NavObserverCallback(nav::NavObserverEvent eventType, const nav::NavCommandState& commandState, void* userData)
{
	const char* eventName = "Unknown";
	switch (eventType)
	{
	case nav::NavObserverEvent::NavCanceled: eventName = "CANCELED"; break;
	case nav::NavObserverEvent::NavPauseChanged: eventName = "PAUSED"; break;
	case nav::NavObserverEvent::NavStarted: eventName = "STARTED"; break;
	case nav::NavObserverEvent::NavDestinationReached: eventName = "DESTINATIONREACHED"; break;
	case nav::NavObserverEvent::NavFailed: eventName = "FAILED"; break;
	default: break;
	}

	SPDLOG_DEBUG("Nav Observer: event=\ag{}\ax tag=\ag{}\ax paused=\ag{}\ax destination=\ag({:.2f}, {:.2f}, {:.2f})\ax type=\ag{}\ax", eventName,
		commandState.tag, commandState.paused, commandState.destination.x, commandState.destination.y, commandState.destination.z,
		commandState.type);

	if (commandState.tag != "easyfind")
		return;

	if (eventType == nav::NavObserverEvent::NavStarted)
	{
		g_activeFindState.valid = true;
	}
	else if (eventType == nav::NavObserverEvent::NavDestinationReached)
	{
		if (g_activeFindState.valid)
		{
			// Determine if we have extra steps to perform once we reach the destination.
			if (g_activeFindState.activateSwitch)
			{
				SPDLOG_DEBUG("Activating switch: \ag{}", g_activeFindState.switchID);

				EQSwitch* pSwitch = GetSwitchByID(g_activeFindState.switchID);
				if (pSwitch)
				{
					pSwitch->UseSwitch(pLocalPlayer->SpawnID, -1, 0, nullptr);
				}
			}

			if (g_activeFindState.findableLocation && !g_activeFindState.findableLocation->luaScript.empty())
			{
				ExecuteLuaScript(g_activeFindState.findableLocation->luaScript, g_activeFindState.findableLocation);
			}

			g_activeFindState.valid = false;
		}
	}
	else if (eventType == nav::NavObserverEvent::NavCanceled)
	{
		if (g_activeFindState.valid)
		{
			g_activeFindState.valid = false;

			ZonePath_NavCanceled();
		}
	}
}

void Navigation_Initialize()
{
	s_nav = (nav::NavAPI*)GetPluginInterface("MQ2Nav");
	if (s_nav)
	{
		s_navObserverId = s_nav->RegisterNavObserver(NavObserverCallback, nullptr);
	}
}

void Navigation_Shutdown()
{
	if (s_nav)
	{
		s_nav->UnregisterNavObserver(s_navObserverId);
		s_navObserverId = 0;
	}

	s_nav = nullptr;
}

void Navigation_Zoned()
{
	// Clear all local navigation state (anything not meant to carry over to the next zone)
	g_activeFindState.valid = {};
}

void Navigation_Reset()
{
	// Clear all existing navigation state
	g_activeFindState = {};
}

void Navigation_BeginZone()
{
	// TODO: Zoning while nav is active counts as a cancel, but maybe it shouldn't.
	g_activeFindState.valid = false;
}

bool Navigation_IsInitialized()
{
	return s_nav != nullptr;
}