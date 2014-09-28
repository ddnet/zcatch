/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
/* zCatch by erd and Teetime                                                                 */
/* Modified by Teelevision for zCatch/TeeVi, see readme.txt and license.txt.                 */
#include <iostream>
#include <engine/shared/config.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include "zcatch.h"

CGameController_zCatch::CGameController_zCatch(class CGameContext *pGameServer) :
		IGameController(pGameServer)
{
	m_pGameType = "zCatch/TeeVi";
	m_OldMode = g_Config.m_SvMode;
}

void CGameController_zCatch::Tick()
{
	IGameController::Tick();

	if(m_OldMode != g_Config.m_SvMode)
	{
		Server()->MapReload();
		m_OldMode = g_Config.m_SvMode;
	}
}

void CGameController_zCatch::DoWincheck()
{
	if(m_GameOverTick == -1)
	{
		int Players = 0, Players_Spec = 0, Players_SpecExplicit = 0;

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(GameServer()->m_apPlayers[i])
			{
				Players++;
				if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS)
					Players_Spec++;
				if(GameServer()->m_apPlayers[i]->m_SpecExplicit)
					Players_SpecExplicit++;
			}
		}
		int Players_Ingame = Players - Players_SpecExplicit;

		if(Players_Ingame <= 1)
		{
			//Do nothing
		}
		else if((Players - Players_Spec) == 1)
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				{
					GameServer()->m_apPlayers[i]->m_Score += g_Config.m_SvBonus;
					if(Players_Ingame < g_Config.m_SvLastStandingPlayers)
						GameServer()->m_apPlayers[i]->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				}
			}
			if(Players_Ingame < g_Config.m_SvLastStandingPlayers)
				GameServer()->SendChatTarget(-1, "Too few players to end round. All players have been released.");
			else
				EndRound();
		}

		IGameController::DoWincheck(); //do also usual wincheck
	}
}

int CGameController_zCatch::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	if(!pKiller)
		return 0;

	CPlayer *victim = pVictim->GetPlayer();
	if(pKiller != victim)
	{
		/* count players playing */
		int numPlayers = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
			if(GameServer()->m_apPlayers[i] && !GameServer()->m_apPlayers[i]->m_SpecExplicit)
				++numPlayers;
		/* you can at max get that many points as there are players playing */
		pKiller->m_Score += min(victim->m_zCatchNumKillsInARow + 1, numPlayers);
		++pKiller->m_Kills;
		++victim->m_Deaths;
		/* Check if the killer is already killed and in spectator (victim may died through wallshot) */
		if(pKiller->GetTeam() != TEAM_SPECTATORS)
		{
			++pKiller->m_zCatchNumKillsInARow;
			pKiller->AddZCatchVictim(victim->GetCID(), CPlayer::ZCATCH_CAUGHT_REASON_KILLED);
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "You are caught until '%s' dies.", Server()->ClientName(pKiller->GetCID()));
			GameServer()->SendChatTarget(victim->GetCID(), aBuf);
		}
	}
	else
	{
		// selfkill/death
		if(WeaponID == WEAPON_SELF || WeaponID == WEAPON_WORLD)
		{
			victim->m_Score -= g_Config.m_SvKillPenalty;
			++victim->m_Deaths;
		}
	}

	// release all the victim's victims
	victim->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
	victim->m_zCatchNumKillsInARow = 0;

	// Update colors
	OnPlayerInfoChange(victim);
	OnPlayerInfoChange(pKiller);

	return 0;
}

void CGameController_zCatch::OnPlayerInfoChange(class CPlayer *pP)
{
    if(g_Config.m_SvColorIndicator)
    {
    	switch(pP->m_zCatchNumKillsInARow){
    		case 0:
				pP->m_TeeInfos.m_ColorBody = 0xFFBB00;
				pP->m_TeeInfos.m_ColorFeet =  0x11FF00;
				break;
    		case 15:
				pP->m_TeeInfos.m_ColorBody = 0xEEFF00;
				break;
    		case 14:
				pP->m_TeeInfos.m_ColorBody = 0xDDFF00;
				break;
    		case 13:
				pP->m_TeeInfos.m_ColorBody = 0xCCFF00;
				break;
    		case 12:
				pP->m_TeeInfos.m_ColorBody = 0xBBFF00;
				break;
    		case 11:
				pP->m_TeeInfos.m_ColorBody = 0xAAFF00;
				break;
    		case 10:
				pP->m_TeeInfos.m_ColorBody = 0x99FF00;
				break;
    		case 9:
				pP->m_TeeInfos.m_ColorBody = 0x88FF00;
				break;
    		case 8:
				pP->m_TeeInfos.m_ColorBody = 0x77FF00;
				break;
    		case 7:
				pP->m_TeeInfos.m_ColorBody = 0x66FF00;
				break;
    		case 6:
				pP->m_TeeInfos.m_ColorBody = 0x55FF00;
				break;
    		case 5:
				pP->m_TeeInfos.m_ColorBody = 0x44FF00;
				break;
    		case 4:
				pP->m_TeeInfos.m_ColorBody = 0x33FF00;
				break;
    		case 3:
				pP->m_TeeInfos.m_ColorBody = 0x22FF00;
				break;
    		case 2:
				pP->m_TeeInfos.m_ColorBody = 0x11FF00;
				break;
    		case 1:
				pP->m_TeeInfos.m_ColorBody = 0x00FF00;
				break;
    		default:
				pP->m_TeeInfos.m_ColorBody = 0x000000;
				break;
    	}
    	pP->m_TeeInfos.m_ColorFeet = 0x11FF00 + pP->m_TeeInfos.m_ColorFeet;
    	pP->m_TeeInfos.m_UseCustomColor = 1;
    }
//	if(g_Config.m_SvColorIndicator){
//		if(pP->m_zCatchNumKillsInARow > 1 || pP->m_zCatchNumKillsInARow < 16)
//		{
//				pP->m_TeeInfos.m_ColorBody = 0x11FF00 + pP->m_TeeInfos.m_ColorBody;
//		}
//		else
//		{
//				pP->m_TeeInfos.m_ColorBody = 0x11FF00;
//		}
//				pP->m_TeeInfos.m_ColorFeet = pP->m_TeeInfos.m_ColorBody;
//				pP->m_TeeInfos.m_UseCustomColor = 1;
//	}
}

void CGameController_zCatch::StartRound()
{
	IGameController::StartRound();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{
			GameServer()->m_apPlayers[i]->m_Kills = 0;
			GameServer()->m_apPlayers[i]->m_Deaths = 0;
			GameServer()->m_apPlayers[i]->m_TicksSpec = 0;
			GameServer()->m_apPlayers[i]->m_TicksIngame = 0;
		}
	}
}

void CGameController_zCatch::OnCharacterSpawn(class CCharacter *pChr)
{
	// default health and armor
	pChr->IncreaseHealth(10);
	if(g_Config.m_SvMode == 2)
		pChr->IncreaseArmor(10);

	// give default weapons
	switch(g_Config.m_SvMode)
	{
	case 1: /* Instagib - Only Riffle */
		pChr->GiveWeapon(WEAPON_RIFLE, -1);
		break;
	case 2: /* All Weapons */
		pChr->GiveWeapon(WEAPON_HAMMER, -1);
		pChr->GiveWeapon(WEAPON_GUN, g_Config.m_SvWeaponsAmmo);
		pChr->GiveWeapon(WEAPON_GRENADE, g_Config.m_SvWeaponsAmmo);
		pChr->GiveWeapon(WEAPON_SHOTGUN, g_Config.m_SvWeaponsAmmo);
		pChr->GiveWeapon(WEAPON_RIFLE, g_Config.m_SvWeaponsAmmo);
		break;
	case 3: /* Hammer */
		pChr->GiveWeapon(WEAPON_HAMMER, -1);
		break;
	case 4: /* Grenade */
		pChr->GiveWeapon(WEAPON_GRENADE, g_Config.m_SvGrenadeEndlessAmmo ? -1 : g_Config.m_SvWeaponsAmmo);
		break;
	case 5: /* Ninja */
		pChr->GiveNinja();
		break;
	}

	//Update color of spawning tees
	OnPlayerInfoChange(pChr->GetPlayer());
}

void CGameController_zCatch::EndRound()
{

	//CheckWinner
	int m_WinnerClientID = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
			if(GameServer()->m_apPlayers[i])
			{
				if(GameServer()->m_apPlayers[i]->m_zCatchNumVictims)
					m_WinnerClientID = GameServer()->m_apPlayers[i]->GetCID();
			}
	}

#if defined(CONF_SQL)
	GameServer()->m_Ranking->SaveRanking(m_WinnerClientID);
#endif
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(GameServer()->m_apPlayers[i])
		{

			if(!GameServer()->m_apPlayers[i]->m_SpecExplicit)
			{
				GameServer()->m_apPlayers[i]->SetTeamDirect(GameServer()->m_pController->ClampTeam(1));

				char aBuf[128];
				str_format(aBuf, sizeof(aBuf), "-----------------------------------");
				GameServer()->SendChatTarget(i, aBuf);
				str_format(aBuf, sizeof(aBuf), "Winner:  %s", Server()->ClientName(m_WinnerClientID));
				GameServer()->SendChatTarget(i, aBuf);
				str_format(aBuf, sizeof(aBuf), "Kills: %d | Deaths: %d", GameServer()->m_apPlayers[i]->m_Kills, GameServer()->m_apPlayers[i]->m_Deaths);
				GameServer()->SendChatTarget(i, aBuf);



				if(GameServer()->m_apPlayers[i]->m_TicksSpec != 0 || GameServer()->m_apPlayers[i]->m_TicksIngame != 0)
				{
					double TimeInSpec = (GameServer()->m_apPlayers[i]->m_TicksSpec * 100.0) / (GameServer()->m_apPlayers[i]->m_TicksIngame + GameServer()->m_apPlayers[i]->m_TicksSpec);
					str_format(aBuf, sizeof(aBuf), "Spec: %.2f%% | Ingame: %.2f%%", (double) TimeInSpec, (double) (100.0 - TimeInSpec));
					GameServer()->SendChatTarget(i, aBuf);
				}
				// release all players
				GameServer()->m_apPlayers[i]->ReleaseZCatchVictim(CPlayer::ZCATCH_RELEASE_ALL);
				GameServer()->m_apPlayers[i]->m_zCatchNumKillsInARow = 0;
			}
		}
	}

	if(m_Warmup) // game can't end when we are running warmup
		return;

	GameServer()->m_World.m_Paused = true;
	m_GameOverTick = Server()->Tick();
	m_SuddenDeath = 0;

}

bool CGameController_zCatch::CanChangeTeam(CPlayer *pPlayer, int JoinTeam)
{
	if(pPlayer->m_CaughtBy >= 0)
		return false;
	return true;
}

bool CGameController_zCatch::OnEntity(int Index, vec2 Pos)
{
	if(Index == ENTITY_SPAWN)
		m_aaSpawnPoints[0][m_aNumSpawnPoints[0]++] = Pos;
	else if(Index == ENTITY_SPAWN_RED)
		m_aaSpawnPoints[1][m_aNumSpawnPoints[1]++] = Pos;
	else if(Index == ENTITY_SPAWN_BLUE)
		m_aaSpawnPoints[2][m_aNumSpawnPoints[2]++] = Pos;

	return false;
}
