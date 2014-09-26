#ifndef GAME_SERVER_RANKING_H
#define GAME_SERVER_RANKING_H

#include "gamecontext.h"

#include <mysql_connection.h>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>


class CRanking
{
	CGameContext *m_pGameServer;
	IServer *m_pServer;


	sql::Driver *m_pDriver;
	sql::Connection *m_pConnection;
	sql::Statement *m_pStatement;
	sql::ResultSet *m_pResults;

	// copy of config vars
	const char* m_pDatabase;
	const char* m_pUser;
	const char* m_pPass;
	const char* m_pIp;
	int m_Port;

	CGameContext *GameServer()
	{
		return m_pGameServer;
	}
	IServer *Server()
	{
		return m_pServer;
	}

	static void SaveRankingThread(void *pUser);

	void Init();

	bool Connect();
	void Disconnect();
	// anti SQL injection
	void ClearString(char *pString, int size = 32);



public:

	CRanking(CGameContext *pGameServer);
	~CRanking();

	void SaveRanking(int ClientID);
};

struct CSqlRankData
{
	CRanking *m_pSqlData;
	int m_ClientID;
#if defined(CONF_FAMILY_WINDOWS)
	char m_aName[16]; // Don't edit this, or all your teeth will fall http://bugs.mysql.com/bug.php?id=50046
#else
	char m_aName[MAX_NAME_LENGTH * 2 - 1];
#endif
};

#endif
