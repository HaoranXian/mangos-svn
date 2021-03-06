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

#ifndef __BATTLEGROUNDWS_H
#define __BATTLEGROUNDWS_H

#include "BattleGround.h"

#define BG_WS_MAX_TEAM_SCORE      3
#define BG_WS_FLAG_RESPAWN_TIME   23000
#define BG_WS_FLAG_DROP_TIME      10000

#define BG_WS_SOUND_FLAG_CAPTURED_ALLIANCE 8173
#define BG_WS_SOUND_FLAG_CAPTURED_HORDE    8213
#define BG_WS_SOUND_FLAG_PLACED            8232

#define BG_WS_SPELL_WARSONG_FLAG      23333
#define BG_WS_SPELL_SILVERWING_FLAG   23335

// WorldStates
#define BG_WS_FLAG_UNK_ALLIANCE       1545
#define BG_WS_FLAG_UNK_HORDE          1546
//#define FLAG_UNK                1547
#define BG_WS_FLAG_CAPTURES_ALLIANCE  1581
#define BG_WS_FLAG_CAPTURES_HORDE     1582
#define BG_WS_FLAG_CAPTURES_MAX       1601
#define BG_WS_FLAG_STATE_HORDE        2338
#define BG_WS_FLAG_STATE_ALLIANCE     2339

class BattleGroundWGScore : public BattleGroundScore
{
    public:
        BattleGroundWGScore() : FlagCaptures(0), FlagReturns(0) {};
        virtual ~BattleGroundWGScore() {};
        uint32 FlagCaptures;
        uint32 FlagReturns;
};

enum BG_WS_ObjectTypes
{
    BG_WS_OBJECT_A_FLAG        = 0,
    BG_WS_OBJECT_H_FLAG        = 1,
    BG_WS_OBJECT_SPEEDBUFF_1   = 2,
    BG_WS_OBJECT_SPEEDBUFF_2   = 3,
    BG_WS_OBJECT_REGENBUFF_1   = 4,
    BG_WS_OBJECT_REGENBUFF_2   = 5,
    BG_WS_OBJECT_BERSERKBUFF_1 = 6,
    BG_WS_OBJECT_BERSERKBUFF_2 = 7,
    BG_WS_OBJECT_DOOR_A_1      = 8,
    BG_WS_OBJECT_DOOR_A_2      = 9,
    BG_WS_OBJECT_DOOR_A_3      = 10,
    BG_WS_OBJECT_DOOR_A_4      = 11,
    BG_WS_OBJECT_DOOR_A_5      = 12,
    BG_WS_OBJECT_DOOR_A_6      = 13,
    BG_WS_OBJECT_DOOR_H_1      = 14,
    BG_WS_OBJECT_DOOR_H_2      = 15,
    BG_WS_OBJECT_DOOR_H_3      = 16,
    BG_WS_OBJECT_DOOR_H_4      = 17,
    BG_WS_OBJECT_MAX           = 18
};

enum BG_WS_ObjectEntry
{
    BG_OBJECT_DOOR_A_1_WS_ENTRY          = 179918,
    BG_OBJECT_DOOR_A_2_WS_ENTRY          = 179919,
    BG_OBJECT_DOOR_A_3_WS_ENTRY          = 179920,
    BG_OBJECT_DOOR_A_4_WS_ENTRY          = 179921,
    BG_OBJECT_DOOR_A_5_WS_ENTRY          = 180322,
    BG_OBJECT_DOOR_A_6_WS_ENTRY          = 180322,
    BG_OBJECT_DOOR_H_1_WS_ENTRY          = 179916,
    BG_OBJECT_DOOR_H_2_WS_ENTRY          = 179917,
    BG_OBJECT_DOOR_H_3_WS_ENTRY          = 180322,
    BG_OBJECT_DOOR_H_4_WS_ENTRY          = 180322,
    BG_OBJECT_A_FLAG_WS_ENTRY            = 179830,
    BG_OBJECT_H_FLAG_WS_ENTRY            = 179831
};

enum BG_WS_FlagState
{
    BG_WS_FLAG_STATE_ON_BASE      = 0,
    BG_WS_FLAG_STATE_WAIT_RESPAWN = 1,
    BG_WS_FLAG_STATE_ON_PLAYER    = 2,
    BG_WS_FLAG_STATE_ON_GROUND    = 3
};

enum BattleGroundGraveyardsWS
{
    WS_GRAVEYARD_MAIN_ALLIANCE   = 771,
    WS_GRAVEYARD_MAIN_HORDE      = 772
};

enum WSBattleGroundCreaturesTypes
{
    WS_SPIRIT_MAIN_ALLIANCE   = 0,
    WS_SPIRIT_MAIN_HORDE      = 1,

    BG_CREATURES_MAX_WS       = 2
};

class BattleGroundWS : public BattleGround
{
    friend class BattleGroundMgr;

    public:
        /* Construction */
        BattleGroundWS();
        ~BattleGroundWS();
        void Update(time_t diff);

        /* inherited from BattlegroundClass */
        virtual void AddPlayer(Player *plr);

        /* BG Flags */
        uint64 GetAllianceFlagPickerGUID() const    { return m_FlagKeepers[BG_TEAM_ALLIANCE]; }
        uint64 GetHordeFlagPickerGUID() const       { return m_FlagKeepers[BG_TEAM_HORDE]; }
        void SetAllianceFlagPicker(uint64 guid)     { m_FlagKeepers[BG_TEAM_ALLIANCE] = guid; }
        void SetHordeFlagPicker(uint64 guid)        { m_FlagKeepers[BG_TEAM_HORDE] = guid; }
        bool IsAllianceFlagPickedup() const         { return m_FlagKeepers[BG_TEAM_ALLIANCE] != 0; }
        bool IsHordeFlagPickedup() const            { return m_FlagKeepers[BG_TEAM_HORDE] != 0; }
        void RespawnFlag(uint32 Team, bool captured);
        void RespawnFlagAfterDrop(uint32 Team);
        uint8 GetFlagState(uint32 team)             { return m_FlagState[GetTeamIndexByTeamId(team)]; }

        /* Battleground Events */
        void EventPlayerCapturedFlag(Player *Source);
        void EventPlayerDroppedFlag(Player *Source);
        void EventPlayerReturnedFlag(Player *Source);
        void EventPlayerPickedUpFlag(Player *Source);

        void RemovePlayer(Player *plr, uint64 guid);
        void HandleAreaTrigger(Player *Source, uint32 Trigger);
        void HandleKillPlayer(Player* player, Player *killer);
        void HandleDropFlag(Player* player);
        bool SetupBattleGround();
        virtual void ResetBGSubclass();

        void UpdateFlagState(uint32 team, uint32 value);
        void UpdateTeamScore(uint32 team);
        void UpdatePlayerScore(Player *Source, uint32 type, uint32 value);
        void SetDropedFlagGUID(uint64 guid, uint32 TeamID)  { m_DropedFlagsGUID[GetTeamIndexByTeamId(TeamID)] = guid;}
        uint64 GetDropedFlagGUID(uint32 TeamID)             { return m_DropedFlagsGUID[GetTeamIndexByTeamId(TeamID)];}
        virtual void FillInitialWorldStates(WorldPacket& data);

        /* Scorekeeping */
        uint32 GetTeamScore(uint32 TeamID) const            { return m_TeamScores[GetTeamIndexByTeamId(TeamID)]; }
        void AddPoint(uint32 TeamID, uint32 Points = 1)     { m_TeamScores[GetTeamIndexByTeamId(TeamID)] += Points; }
        void SetTeamPoint(uint32 TeamID, uint32 Points = 0) { m_TeamScores[GetTeamIndexByTeamId(TeamID)] = Points; }
        void RemovePoint(uint32 TeamID, uint32 Points = 1)  { m_TeamScores[GetTeamIndexByTeamId(TeamID)] -= Points; }

    private:
        uint64 m_FlagKeepers[2];                            // 0 - alliance, 1 - horde
        uint64 m_DropedFlagsGUID[2];
        uint8 m_FlagState[2];                               // for checking flag state
        uint32 m_TeamScores[2];
        int32 m_FlagsTimer[2];
        int32 m_FlagsDropTimer[2];
};
#endif
