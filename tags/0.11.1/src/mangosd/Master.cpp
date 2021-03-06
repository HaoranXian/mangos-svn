/* 
 * Copyright (C) 2005-2008 MaNGOS <http://www.mangosproject.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/** \file
    \ingroup mangosd
*/

#include "Master.h"
#include "sockets/SocketHandler.h"
#include "sockets/ListenSocket.h"
#include "WorldSocket.h"
#include "WorldSocketMgr.h"
#include "WorldRunnable.h"
#include "World.h"
#include "Log.h"
#include "Timer.h"
#include <signal.h>
#include "Policies/SingletonImp.h"
#include "SystemConfig.h"
#include "Config/ConfigEnv.h"
#include "Database/DatabaseEnv.h"
#include "CliRunnable.h"
#include "RASocket.h"
#include "ScriptCalls.h"
#include "Util.h"

#include "sockets/TcpSocket.h"
#include "sockets/Utility.h"
#include "sockets/Parse.h"
#include "sockets/Socket.h"

#ifdef WIN32
#include "ServiceWin32.h"
extern int m_ServiceStatus;
#endif

/// \todo Warning disabling not useful under VC++2005. Can somebody say on which compiler it is useful?
#pragma warning(disable:4305)

INSTANTIATE_SINGLETON_1( Master );

volatile uint32 Master::m_masterLoopCounter = 0;

class FreezeDetectorRunnable : public ZThread::Runnable
{
public:
    FreezeDetectorRunnable() { _delaytime = 0; }
    uint32 m_loops, m_lastchange;
    uint32 w_loops, w_lastchange;
    uint32 _delaytime;
    void SetDelayTime(uint32 t) { _delaytime = t; }
    void run(void)
    {
        if(!_delaytime)
            return;
        sLog.outString("Starting up anti-freeze thread (%u seconds max stuck time)...",_delaytime/1000);
        m_loops = 0;
        w_loops = 0;
        m_lastchange = 0;
        w_lastchange = 0;
        while(!World::m_stopEvent)
        {
            ZThread::Thread::sleep(1000);
            uint32 curtime = getMSTime();
            //DEBUG_LOG("anti-freeze: time=%u, counters=[%u; %u]",curtime,Master::m_masterLoopCounter,World::m_worldLoopCounter);

            // normal work
            if(m_loops != Master::m_masterLoopCounter)
            {
                m_lastchange = curtime;
                m_loops = Master::m_masterLoopCounter;
            }
            // possible freeze 
            else if(getMSTimeDiff(m_lastchange,curtime) > _delaytime)
            {
                sLog.outError("Main/Sockets Thread hangs, kicking out server!");
                *((uint32 volatile*)NULL) = 0;                       // bang crash
            }

            // normal work
            if(w_loops != World::m_worldLoopCounter)
            {
                w_lastchange = curtime;
                w_loops = World::m_worldLoopCounter;
            }
            // possible freeze 
            else if(getMSTimeDiff(w_lastchange,curtime) > _delaytime)
            {
                sLog.outError("World Thread hangs, kicking out server!");
                *((uint32 volatile*)NULL) = 0;                       // bang crash
            }
        }
        sLog.outString("Anti-freeze thread exiting without problems.");
    }
};


Master::Master()
{
}

Master::~Master()
{
}

/// Main function
void Master::Run()
{
    sLog.outString( "%s (world-daemon)", _FULLVERSION );
    sLog.outString( "<Ctrl-C> to stop.\n\n" );

    sLog.outTitle( "MM   MM         MM   MM  MMMMM   MMMM   MMMMM");
    sLog.outTitle( "MM   MM         MM   MM MMM MMM MM  MM MMM MMM");
    sLog.outTitle( "MMM MMM         MMM  MM MMM MMM MM  MM MMM");
    sLog.outTitle( "MM M MM         MMMM MM MMM     MM  MM  MMM");
    sLog.outTitle( "MM M MM  MMMMM  MM MMMM MMM     MM  MM   MMM");
    sLog.outTitle( "MM M MM M   MMM MM  MMM MMMMMMM MM  MM    MMM");
    sLog.outTitle( "MM   MM     MMM MM   MM MM  MMM MM  MM     MMM");
    sLog.outTitle( "MM   MM MMMMMMM MM   MM MMM MMM MM  MM MMM MMM");
    sLog.outTitle( "MM   MM MM  MMM MM   MM  MMMMMM  MMMM   MMMMM");
    sLog.outTitle( "        MM  MMM http://www.mangosproject.org");
    sLog.outTitle( "        MMMMMM\n\n");

    /// worldd PID file creation
    std::string pidfile = sConfig.GetStringDefault("PidFile", "");
    if(!pidfile.empty())
    {
        uint32 pid = CreatePIDFile(pidfile);
        if( !pid )
        {
            sLog.outError( "Cannot create PID file %s.\n", pidfile.c_str() );
            return;
        }

        sLog.outString( "Daemon PID: %u\n", pid );
    }

    ///- Start the databases
    if (!_StartDB())
        return;

    ///- Initialize the World
    sWorld.SetInitialWorldSettings();

    ///- Launch the world listener socket
    port_t wsport = sWorld.getConfig(CONFIG_PORT_WORLD);
    std::string bind_ip = sConfig.GetStringDefault("BindIP", "0.0.0.0");

    SocketHandler h;
    ListenSocket<WorldSocket> worldListenSocket(h);
    if (worldListenSocket.Bind(bind_ip.c_str(),wsport))
    {
        clearOnlineAccounts();
        sLog.outError("MaNGOS cannot bind to %s:%d",bind_ip.c_str(), wsport);
        return;
    }

    h.Add(&worldListenSocket);

    ///- Catch termination signals
    _HookSignals();

    ///- Launch WorldRunnable thread
    ZThread::Thread t(new WorldRunnable);
    t.setPriority ((ZThread::Priority )2);

    // set server online
    loginDatabase.PExecute("UPDATE realmlist SET color = 0, population = 0 WHERE id = '%d'",realmID);

#ifdef WIN32
    if (sConfig.GetBoolDefault("Console.Enable", true) && (m_ServiceStatus == -1)/* need disable console in service mode*/)
#else
    if (sConfig.GetBoolDefault("Console.Enable", true))
#endif
    {
        ///- Launch CliRunnable thread
        ZThread::Thread td1(new CliRunnable);
    }

    ///- Launch the RA listener socket
    ListenSocket<RASocket> RAListenSocket(h);
    if (sConfig.GetBoolDefault("Ra.Enable", false))
    {
        port_t raport = sConfig.GetIntDefault( "Ra.Port", 3443 );
        std::string stringip = sConfig.GetStringDefault( "Ra.IP", "0.0.0.0" );
        ipaddr_t raip;
        if(!Utility::u2ip(stringip, raip))
            sLog.outError( "MaNGOS RA can not bind to ip %s", stringip.c_str());
        else if (RAListenSocket.Bind(raip, raport))
            sLog.outError( "MaNGOS RA can not bind to port %d on %s", raport, stringip.c_str());
        else
        {
            h.Add(&RAListenSocket);

            sLog.outString("Starting Remote access listner on port %d on %s", raport, stringip.c_str());
        }
    }

    ///- Handle affinity for multiple processors and process priority on Windows
    #ifdef WIN32
    {
        HANDLE hProcess = GetCurrentProcess();

        uint32 Aff = sConfig.GetIntDefault("UseProcessors", 0);
        if(Aff > 0)
        {
            ULONG_PTR appAff;
            ULONG_PTR sysAff;

            if(GetProcessAffinityMask(hProcess,&appAff,&sysAff))
            {
                ULONG_PTR curAff = Aff & appAff;            // remove non accessible processors

                if(!curAff )
                {
                    sLog.outError("Processors marked in UseProcessors bitmask (hex) %x not accessible for mangosd. Accessible processors bitmask (hex): %x",Aff,appAff);
                }
                else
                {
                    if(SetProcessAffinityMask(hProcess,curAff))
                        sLog.outString("Using processors (bitmask, hex): %x", curAff);
                    else
                        sLog.outError("Can't set used processors (hex): %x",curAff);
                }
            }
            sLog.outString();
        }

        bool Prio = sConfig.GetBoolDefault("ProcessPriority", false);

//        if(Prio && (m_ServiceStatus == -1)/* need set to default process priority class in service mode*/)
        if(Prio)
        {
            if(SetPriorityClass(hProcess,HIGH_PRIORITY_CLASS))
                sLog.outString("mangosd process priority class set to HIGH");
            else
                sLog.outError("ERROR: Can't set mangosd process priority class.");
            sLog.outString();
        }
    }
    #endif

    uint32 realCurrTime, realPrevTime;
    realCurrTime = realPrevTime = getMSTime();

    uint32 socketSelecttime = sWorld.getConfig(CONFIG_SOCKET_SELECTTIME);

    // maximum counter for next ping
    uint32 numLoops = (sConfig.GetIntDefault( "MaxPingTime", 30 ) * (MINUTE * 1000000 / socketSelecttime));
    uint32 loopCounter = 0;

    ///- Start up freeze catcher thread
    uint32 freeze_delay = sConfig.GetIntDefault("MaxCoreStuckTime", 0);
    if(freeze_delay)
    {
        FreezeDetectorRunnable *fdr = new FreezeDetectorRunnable();
        fdr->SetDelayTime(freeze_delay*1000);
        ZThread::Thread t(fdr);
        t.setPriority(ZThread::High);
    }

    ///- Wait for termination signal
    while (!World::m_stopEvent)
    {
        ++Master::m_masterLoopCounter;
#ifdef WIN32
        if (m_ServiceStatus == 0) World::m_stopEvent = true;
        while (m_ServiceStatus == 2) Sleep(1000);
#endif
        if (realPrevTime > realCurrTime)
            realPrevTime = 0;

        realCurrTime = getMSTime();
        sWorldSocketMgr.Update( getMSTimeDiff(realPrevTime,realCurrTime) );
        realPrevTime = realCurrTime;

        h.Select(0, socketSelecttime);

        // ping if need
        if( (++loopCounter) == numLoops )
        {
            loopCounter = 0;
            sLog.outDetail("Ping MySQL to keep connection alive");
            delete WorldDatabase.Query("SELECT 1 FROM command LIMIT 1");
            delete loginDatabase.Query("SELECT 1 FROM realmlist LIMIT 1");
            delete CharacterDatabase.Query("SELECT 1 FROM bugreport LIMIT 1");
        }
    }

    // set server offline
    loginDatabase.PExecute("UPDATE realmlist SET color = 2 WHERE id = '%d'",realmID);

    ///- Remove signal handling before leaving
    _UnhookSignals();

    // when the main thread closes the singletons get unloaded
    // since worldrunnable uses them, it will crash if unloaded after master
    t.wait();

    ///- Clean database before leaving
    clearOnlineAccounts();

    ///- Wait for delay threads to end
    CharacterDatabase.HaltDelayThread();
    WorldDatabase.HaltDelayThread();
    loginDatabase.HaltDelayThread();

    sLog.outString( "Halting process..." );

    #ifdef WIN32
    if (sConfig.GetBoolDefault("Console.Enable", true))
    {
        // this only way to terminate CLI thread exist at Win32 (alt. way exist only in Windows Vista API)
        //_exit(1);
        // send keyboard input to safely unblock the CLI thread
        INPUT_RECORD b[5];
        HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        b[0].EventType = KEY_EVENT;
        b[0].Event.KeyEvent.bKeyDown = TRUE;
        b[0].Event.KeyEvent.uChar.AsciiChar = 'X';
        b[0].Event.KeyEvent.wVirtualKeyCode = 'X';
        b[0].Event.KeyEvent.wRepeatCount = 1;

        b[1].EventType = KEY_EVENT;
        b[1].Event.KeyEvent.bKeyDown = FALSE;
        b[1].Event.KeyEvent.uChar.AsciiChar = 'X';
        b[1].Event.KeyEvent.wVirtualKeyCode = 'X';
        b[1].Event.KeyEvent.wRepeatCount = 1;

        b[2].EventType = KEY_EVENT;
        b[2].Event.KeyEvent.bKeyDown = TRUE;
        b[2].Event.KeyEvent.dwControlKeyState = 0;
        b[2].Event.KeyEvent.uChar.AsciiChar = '\r';
        b[2].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
        b[2].Event.KeyEvent.wRepeatCount = 1;
        b[2].Event.KeyEvent.wVirtualScanCode = 0x1c;

        b[3].EventType = KEY_EVENT;
        b[3].Event.KeyEvent.bKeyDown = FALSE;
        b[3].Event.KeyEvent.dwControlKeyState = 0;
        b[3].Event.KeyEvent.uChar.AsciiChar = '\r';
        b[3].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
        b[3].Event.KeyEvent.wVirtualScanCode = 0x1c;
        b[3].Event.KeyEvent.wRepeatCount = 1;
        DWORD numb;
        BOOL ret = WriteConsoleInput(hStdIn, b, 4, &numb);
    }
    #endif

    // for some unknown reason, unloading scripts here and not in worldrunnable
    // fixes a memory leak related to detaching threads from the module
    UnloadScriptingModule();

    return;
}

/// Initialize connection to the databases
bool Master::_StartDB()
{
    ///- Get world database info from configuration file
    std::string dbstring;
    if(!sConfig.GetString("WorldDatabaseInfo", &dbstring))
    {
        sLog.outError("Database not specified in configuration file");
        return false;
    }
    sLog.outString("World Database: %s", dbstring.c_str());

    ///- Initialise the world database
    if(!WorldDatabase.Initialize(dbstring.c_str()))
    {
        sLog.outError("Cannot connect to world database %s",dbstring.c_str());
        return false;
    }

    if(!sConfig.GetString("CharacterDatabaseInfo", &dbstring))
    {
        sLog.outError("Character Database not specified in configuration file");
        return false;
    }
    sLog.outString("Character Database: %s", dbstring.c_str());

    ///- Initialise the Character database
    if(!CharacterDatabase.Initialize(dbstring.c_str()))
    {
        sLog.outError("Cannot connect to Character database %s",dbstring.c_str());
        return false;
    }

    ///- Get login database info from configuration file
    if(!sConfig.GetString("LoginDatabaseInfo", &dbstring))
    {
        sLog.outError("Login database not specified in configuration file");
        return false;
    }

    ///- Initialise the login database
    sLog.outString("Login Database: %s", dbstring.c_str() );
    if(!loginDatabase.Initialize(dbstring.c_str()))
    {
        sLog.outError("Cannot connect to login database %s",dbstring.c_str());
        return false;
    }

    ///- Get the realm Id from the configuration file
    realmID = sConfig.GetIntDefault("RealmID", 0);
    if(!realmID)
    {
        sLog.outError("Realm ID not defined in configuration file");
        return false;
    }
    sLog.outString("Realm running as realm ID %d", realmID);

    ///- Clean the database before starting
    clearOnlineAccounts();

    QueryResult* result = WorldDatabase.Query("SELECT version FROM db_version LIMIT 1");
    if(result)
    {
        Field* fields = result->Fetch();

        sLog.outString("Using %s", fields[0].GetString());
        delete result;
    }
    else
        sLog.outString("Using unknown world database.");

    return true;
}

/// Clear 'online' status for all accounts with characters in this realm
void Master::clearOnlineAccounts()
{
    // Cleanup online status for characters hosted at current realm
    /// \todo Only accounts with characters logged on *this* realm should have online status reset. Move the online column from 'account' to 'realmcharacters'?
    loginDatabase.PExecute(
        "UPDATE account SET online = 0 WHERE online > 0 "
        "AND id IN (SELECT acctid FROM realmcharacters WHERE realmid = '%d')",realmID);
    

    CharacterDatabase.Execute("UPDATE characters SET online = 0");
}

/// Handle termination signals
/** Put the World::m_stopEvent to 'true' if a termination signal is caught **/
void Master::_OnSignal(int s)
{
    switch (s)
    {
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
        case SIGABRT:
        #ifdef _WIN32
        case SIGBREAK:
        #endif
            World::m_stopEvent = true;
            break;
    }

    signal(s, _OnSignal);
}

/// Define hook '_OnSignal' for all termination signals
void Master::_HookSignals()
{
    signal(SIGINT, _OnSignal);
    signal(SIGQUIT, _OnSignal);
    signal(SIGTERM, _OnSignal);
    signal(SIGABRT, _OnSignal);
    #ifdef _WIN32
    signal(SIGBREAK, _OnSignal);
    #endif
}

/// Unhook the signals before leaving
void Master::_UnhookSignals()
{
    signal(SIGINT, 0);
    signal(SIGQUIT, 0);
    signal(SIGTERM, 0);
    signal(SIGABRT, 0);
    #ifdef _WIN32
    signal(SIGBREAK, 0);
    #endif
}








