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

			dbg_msg("SQL", "Ranking table were created successfully");
		}
		catch (sql::SQLException &e)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "MySQL Error: %s", e.what());
			dbg_msg("SQL", aBuf);
			dbg_msg("SQL", "ERROR: Tables were NOT created");
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
		dbg_msg("SQL", "Unknown Error cause by the MySQL/C++ Connector, my advice compile server_debug and use it");

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
						str_format(aBuf2, sizeof(aBuf), "You're winner! Now you have %d wins!", wins);
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
