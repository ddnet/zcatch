#if defined(CONF_SQL)
#include <string.h>
#include <algorithm>

#include "ranking.h"
#include <engine/shared/config.h>

static LOCK gs_SqlLock = 0;

CRanking::CRanking(CGameContext *pGameServer) : m_pGameServer(pGameServer),
		m_pServer(pGameServer->Server()),
		m_pDatabase(g_Config.m_SvSqlDatabase),
		m_pUser(g_Config.m_SvSqlUser),
		m_pPass(g_Config.m_SvSqlPass),
		m_pIp(g_Config.m_SvSqlIp),
		m_Port(g_Config.m_SvSqlPort)
{
	m_pDriver = NULL;

	if(gs_SqlLock == 0)
		gs_SqlLock = lock_create();

	Init();
}

CRanking::~CRanking()
{
	lock_wait(gs_SqlLock);
	lock_release(gs_SqlLock);

	try
	{
		delete m_pStatement;
		delete m_pConnection;
		dbg_msg("SQL", "SQL connection disconnected");
	}
	catch (sql::SQLException &e)
	{
		dbg_msg("SQL", "ERROR: No SQL connection");
	}
}
void CRanking::Init()
{
	// create connection
	if(Connect())
	{
		try
		{
			// create tables
			char aBuf[1024];

			str_format(aBuf, sizeof(aBuf), "CREATE TABLE IF NOT EXISTS zcatch_ranks (Name VARCHAR(%d) BINARY NOT NULL, Wins INT DEFAULT 0, UNIQUE KEY Name (Name)) CHARACTER SET utf8 ;", MAX_NAME_LENGTH);
			m_pStatement->execute(aBuf);

			dbg_msg("SQL", "Ranking table was created successfully");
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Table was NOT created");
		}

		// disconnect from database
		Disconnect();
	}
}

bool CRanking::Connect()
{
	if (m_pDriver != NULL && m_pConnection != NULL)
	{
		try
		{
			// Connect to specific database
			m_pConnection->setSchema(m_pDatabase);
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);

			dbg_msg("SQL", "ERROR: SQL connection failed");
			return false;
		}
		return true;
	}

	try
	{
		char aBuf[256];

		m_pDriver = 0;
		m_pConnection = 0;
		m_pStatement = 0;

		sql::ConnectOptionsMap connection_properties;
		connection_properties["hostName"]      = sql::SQLString(m_pIp);
		connection_properties["port"]          = m_Port;
		connection_properties["userName"]      = sql::SQLString(m_pUser);
		connection_properties["password"]      = sql::SQLString(m_pPass);
		connection_properties["OPT_RECONNECT"] = true;

		// Create connection
		m_pDriver = get_driver_instance();
		m_pConnection = m_pDriver->connect(connection_properties);

		// Create Statement
		m_pStatement = m_pConnection->createStatement();

		// Create database if not exists
		str_format(aBuf, sizeof(aBuf), "CREATE DATABASE IF NOT EXISTS %s", m_pDatabase);
		m_pStatement->execute(aBuf);

		// Connect to specific database
		m_pConnection->setSchema(m_pDatabase);
		dbg_msg("SQL", "SQL connection established");
		return true;
	}
	catch (sql::SQLException &e)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
		dbg_msg("SQL", aBuf);

		dbg_msg("SQL", "ERROR: SQL connection failed");
		return false;
	}
	catch (const std::exception& ex)
	{
		// ...
		dbg_msg("SQL", "1 %s",ex.what());

	}
	catch (const std::string& ex)
	{
		// ...
		dbg_msg("SQL", "2 %s",ex.c_str());
	}
	catch( int )
	{
		dbg_msg("SQL", "3 %s");
	}
	catch( float )
	{
		dbg_msg("SQL", "4 %s");
	}

	catch( char[] )
	{
		dbg_msg("SQL", "5 %s");
	}

	catch( char )
	{
		dbg_msg("SQL", "6 %s");
	}
	catch (...)
	{
		dbg_msg("SQL", "Unknown Error cause by the MySQL/C++ Connector, my advice is to compile server_sql_debug and use it");

		dbg_msg("SQL", "ERROR: SQL connection failed");
		return false;
	}
	return false;
}
void CRanking::Disconnect()
{
}

void CRanking::SaveRanking(int ClientID){

	CSqlRankData *Tmp = new CSqlRankData();
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, Server()->ClientName(ClientID), MAX_NAME_LENGTH);
	Tmp->m_pSqlData = this;

	void *SaveRanking = thread_create(SaveRankingThread, Tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)SaveRanking);
#endif
}
void CRanking::SaveRankingThread(void *pUser){

	lock_wait(gs_SqlLock);
	CSqlRankData *pData = (CSqlRankData *)pUser;

	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			char aBuf[768];
			char aBuf2[768];
			// check strings
			pData->m_pSqlData->ClearString(pData->m_aName);

			str_format(aBuf, sizeof(aBuf), "SELECT * FROM zcatch_ranks WHERE Name='%s' ORDER BY Wins ASC LIMIT 1;", pData->m_aName);
			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);

			if(pData->m_pSqlData->m_pResults->next())
			{
				str_format(aBuf, sizeof(aBuf), "SELECT Wins FROM zcatch_ranks WHERE Name ='%s'",pData->m_aName);
				pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);

				if(pData->m_pSqlData->m_pResults->rowsCount() == 1){
					pData->m_pSqlData->m_pResults->next();
					int wins = (int)pData->m_pSqlData->m_pResults->getInt("Wins");

					str_format(aBuf, sizeof(aBuf), "INSERT INTO zcatch_ranks(Name, Wins) VALUES ('%s', '%d') ON duplicate key UPDATE Name=VALUES(Name), Wins=Wins+1;", pData->m_aName, wins);
					pData->m_pSqlData->m_pStatement->execute(aBuf);

					if(wins == 1){
						str_format(aBuf2, sizeof(aBuf), "Woah! You won for the first time! Now you have 1 win.");
						pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf2);
					}else{
						str_format(aBuf2, sizeof(aBuf), "You're the winner! Now you have %d wins!", wins+1);
						pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf2);
					}
				}
			}

			delete pData->m_pSqlData->m_pResults;

			// if no entry found... create a new one
			str_format(aBuf, sizeof(aBuf), "INSERT IGNORE INTO zcatch_ranks(Name, Wins) VALUES ('%s', '1');",pData->m_aName);
			pData->m_pSqlData->m_pStatement->execute(aBuf);

			dbg_msg("SQL", "Updating rank done");
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Could not update rank");
		}
		pData->m_pSqlData->Disconnect();
	}

	delete pData;
	lock_release(gs_SqlLock);
}


void CRanking::ShowRanking(int ClientID, const char* pName){

	CSqlRankData *Tmp = new CSqlRankData();
	Tmp->m_ClientID = ClientID;
	str_copy(Tmp->m_aName, pName, MAX_NAME_LENGTH);
	str_format(Tmp->m_aRequestingPlayer, sizeof(Tmp->m_aRequestingPlayer), "%s", Server()->ClientName(ClientID));
	Tmp->m_pSqlData = this;

	void *ShowRanking = thread_create(ShowRankingThread, Tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)ShowRanking);
#endif
}

void CRanking::ShowRankingThread(void *pUser){

	lock_wait(gs_SqlLock);
	CSqlRankData *pData = (CSqlRankData *)pUser;

	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{
			// check strings

			char originalName[MAX_NAME_LENGTH];
			strcpy(originalName,pData->m_aName);
			pData->m_pSqlData->ClearString(pData->m_aName);

			char aBuf[768];
			char aBuf2[768];
			pData->m_pSqlData->m_pStatement->execute("SET @pos := 0;");
			pData->m_pSqlData->m_pStatement->execute("SET @prev := NULL;");
			pData->m_pSqlData->m_pStatement->execute("SET @rank := 1;");

			str_format(aBuf, sizeof(aBuf), "SELECT Wins,Name, rank FROM (SELECT (@pos := @pos+1) pos, (@rank := IF(@prev = Wins,@rank, @pos)) rank, Name, (@prev := Wins) Wins FROM (SELECT Name, Wins FROM zcatch_ranks ORDER BY Wins DESC Limit 18446744073709551615) as a) as b WHERE Name='%s';", pData->m_aName);
			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);

			if(pData->m_pSqlData->m_pResults->next())
			{
				if(pData->m_pSqlData->m_pResults->rowsCount() == 1){
					int wins = (int)pData->m_pSqlData->m_pResults->getInt("Wins");
					int rank = (int)pData->m_pSqlData->m_pResults->getInt("rank");
					str_format(aBuf2, sizeof(aBuf2), "%d. %s has %d %s, requested by %s.", rank,originalName,wins, (wins != 1) ? "wins" : "win", pData->m_aRequestingPlayer);
					pData->m_pSqlData->GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf2);
				}
			}else{
				str_format(aBuf2, sizeof(aBuf2), "%s is not ranked.", originalName);
				pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf2);

			}

			// delete results and statement
			delete pData->m_pSqlData->m_pResults;
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Could not update rank");
		}
		pData->m_pSqlData->Disconnect();
	}

	delete pData;
	lock_release(gs_SqlLock);
}


void CRanking::ShowTop5(int ClientID, int Offset){

	CSqlTop5Data *Tmp = new CSqlTop5Data();
	Tmp->m_ClientID = ClientID;
	Tmp->m_Offset = Offset;
	str_format(Tmp->m_aRequestingPlayer, sizeof(Tmp->m_aRequestingPlayer), "%s", Server()->ClientName(ClientID));
	Tmp->m_pSqlData = this;

	void *ShowTop5 = thread_create(ShowTop5Thread, Tmp);
#if defined(CONF_FAMILY_UNIX)
	pthread_detach((pthread_t)ShowTop5);
#endif
}

void CRanking::ShowTop5Thread(void *pUser){

	lock_wait(gs_SqlLock);
	CSqlTop5Data *pData = (CSqlTop5Data *)pUser;

	// Connect to database
	if(pData->m_pSqlData->Connect())
	{
		try
		{

			char aBuf[768];
			char aBuf2[768];

			str_format(aBuf, sizeof(aBuf2), "SELECT Name FROM zcatch_ranks;");
			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);
			str_format(aBuf2, sizeof(aBuf2), "------=====] Top5 (%d records)", (int)pData->m_pSqlData->m_pResults->rowsCount());

			pData->m_pSqlData->m_pStatement->execute("SET @pos := 0;");
			pData->m_pSqlData->m_pStatement->execute("SET @prev := NULL;");
			pData->m_pSqlData->m_pStatement->execute("SET @rank := 1;");

			str_format(aBuf, sizeof(aBuf), "SELECT Wins,Name, rank FROM (SELECT (@pos := @pos+1) pos, (@rank := IF(@prev = Wins,@rank, @pos)) rank, Name, (@prev := Wins) Wins FROM (SELECT Name, Wins FROM zcatch_ranks ORDER BY Wins DESC Limit 18446744073709551615) as a) as b ORDER BY rank Limit %d,5;", pData->m_Offset);
			pData->m_pSqlData->m_pResults = pData->m_pSqlData->m_pStatement->executeQuery(aBuf);


			pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf2);
			while(pData->m_pSqlData->m_pResults->next()){
					int wins = (int)pData->m_pSqlData->m_pResults->getInt("Wins");
					int rank = (int)pData->m_pSqlData->m_pResults->getInt("rank");
					str_format(aBuf2, sizeof(aBuf2), "%d. %s has %d %s", rank, pData->m_pSqlData->m_pResults->getString("Name").c_str(),wins, (wins != 1) ? "wins" : "win");
					pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf2);
			}
			str_format(aBuf2, sizeof(aBuf2), "------=====]");
			pData->m_pSqlData->GameServer()->SendChatTarget(pData->m_ClientID, aBuf2);


			// delete results and statement
			delete pData->m_pSqlData->m_pResults;
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Could not update rank");
		}
		pData->m_pSqlData->Disconnect();
	}

	delete pData;
	lock_release(gs_SqlLock);
}

// anti SQL injection
void CRanking::ClearString(char *pString, int size)
{
	char newString[size*2-1];
	int pos = 0;

	for(int i=0;i<size;i++)
	{
		if(pString[i] == '\\')
		{
			newString[pos++] = '\\';
			newString[pos++] = '\\';
		}
		else if(pString[i] == '\'')
		{
			newString[pos++] = '\\';
			newString[pos++] = '\'';
		}
		else if(pString[i] == '"')
		{
			newString[pos++] = '\\';
			newString[pos++] = '"';
		}
		else
		{
			newString[pos++] = pString[i];
		}
	}

	newString[pos] = '\0';

	strcpy(pString,newString);
}
#endif
