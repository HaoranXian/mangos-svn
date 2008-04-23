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

#include "SocialMgr.h"
#include "Policies/SingletonImp.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "World.h"

INSTANTIATE_SINGLETON_1( SocialMgr );

PlayerSocial::PlayerSocial()
{
    m_playerGUID = 0;
}

PlayerSocial::~PlayerSocial()
{
    m_playerSocialMap.clear();
}

bool PlayerSocial::AddToSocialList(uint32 friend_guid, bool ignore)
{
    // prevent list (client-side) overflow
    if(m_playerSocialMap.size() >= (255-1))
        return false;

    uint32 flag = SOCIAL_FLAG_FRIEND;
    if(ignore)
        flag = SOCIAL_FLAG_IGNORED;

    PlayerSocialMap::iterator itr = m_playerSocialMap.find(friend_guid);
    if(itr != m_playerSocialMap.end())
    {
        CharacterDatabase.PExecute("UPDATE character_social SET flags = (flags | %u) WHERE guid = '%u' AND friend = '%u'", flag, GetPlayerGUID(), friend_guid);
        m_playerSocialMap[friend_guid].Flags |= flag;
    }
    else
    {
        CharacterDatabase.PExecute("INSERT INTO character_social (guid, friend, flags) VALUES ('%u', '%u', '%u')", GetPlayerGUID(), friend_guid, flag);
        FriendInfo fi;
        fi.Flags |= flag;
        m_playerSocialMap[friend_guid] = fi;
    }
    return true;
}

void PlayerSocial::RemoveFromSocialList(uint32 friend_guid, bool ignore)
{
    PlayerSocialMap::iterator itr = m_playerSocialMap.find(friend_guid);
    if(itr == m_playerSocialMap.end())                      // not exist
        return;

    uint32 flag = SOCIAL_FLAG_FRIEND;
    if(ignore)
        flag = SOCIAL_FLAG_IGNORED;

    itr->second.Flags &= ~flag;
    if(itr->second.Flags == 0)
    {
        CharacterDatabase.PExecute("DELETE FROM character_social WHERE guid = '%u' AND friend = '%u'", GetPlayerGUID(), friend_guid);
        m_playerSocialMap.erase(itr);
    }
    else
    {
        CharacterDatabase.PExecute("UPDATE character_social SET flags = (flags & ~%u) WHERE guid = '%u' AND friend = '%u'", flag, GetPlayerGUID(), friend_guid);
    }
}

void PlayerSocial::SetFriendNote(uint32 friend_guid, std::string note)
{
    PlayerSocialMap::iterator itr = m_playerSocialMap.find(friend_guid);
    if(itr == m_playerSocialMap.end())                      // not exist
        return;

    CharacterDatabase.PExecute("UPDATE character_social SET note = '%s' WHERE guid = '%u' AND friend = '%u'", note.c_str(), GetPlayerGUID(), friend_guid);
    m_playerSocialMap[friend_guid].Note = note;
}

void PlayerSocial::SendSocialList()
{
    Player *plr = objmgr.GetPlayer(GetPlayerGUID());
    if(!plr)
        return;

    uint32 size = m_playerSocialMap.size();

    WorldPacket data(SMSG_FRIEND_LIST, (4+4+size*25));      // just can guess size
    data << uint32(7);                                      // unk flag (0x1, 0x2, 0x4), 0x7 if it include ignore list
    data << uint32(size);                                   // friends count

    for(PlayerSocialMap::iterator itr = m_playerSocialMap.begin(); itr != m_playerSocialMap.end(); ++itr)
    {
        sSocialMgr.GetFriendInfo(plr, itr->first, itr->second);

        data << uint64(itr->first);                         // player guid
        data << uint32(itr->second.Flags);                  // player flag (0x1-friend?, 0x2-ignored?, 0x4-muted?)
        data << itr->second.Note;                           // string note
        if(itr->second.Flags & SOCIAL_FLAG_FRIEND)          // if IsFriend()
        {
            data << uint8(itr->second.Status);              // online/offline/etc?
            if(itr->second.Status)                          // if online
            {
                data << uint32(itr->second.Area);           // player area
                data << uint32(itr->second.Level);          // player level
                data << uint32(itr->second.Class);          // player class
            }
        }
    }

    plr->GetSession()->SendPacket(&data);
    sLog.outDebug("WORLD: Sent SMSG_FRIEND_LIST");
}

bool PlayerSocial::HasFriend(uint32 friend_guid)
{
    PlayerSocialMap::iterator itr = m_playerSocialMap.find(friend_guid);
    if(itr != m_playerSocialMap.end())
        return itr->second.Flags & SOCIAL_FLAG_FRIEND;
    return false;
}

bool PlayerSocial::HasIgnore(uint32 ignore_guid)
{
    PlayerSocialMap::iterator itr = m_playerSocialMap.find(ignore_guid);
    if(itr != m_playerSocialMap.end())
        return itr->second.Flags & SOCIAL_FLAG_IGNORED;
    return false;
}

SocialMgr::SocialMgr()
{

}

SocialMgr::~SocialMgr()
{

}

void SocialMgr::RemovePlayerSocial(uint32 guid)
{
    SocialMap::iterator itr = m_socialMap.find(guid);
    if(itr != m_socialMap.end())
        m_socialMap.erase(itr);
}

void SocialMgr::GetFriendInfo(Player *player, uint32 friendGUID, FriendInfo &friendInfo)
{
    if(!player)
        return;

    Player *pFriend = ObjectAccessor::FindPlayer(friendGUID);

    uint32 team = player->GetTeam();
    uint32 security = player->GetSession()->GetSecurity();
    bool allowTwoSideWhoList = sWorld.getConfig(CONFIG_ALLOW_TWO_SIDE_WHO_LIST);
    bool gmInWhoList = sWorld.getConfig(CONFIG_GM_IN_WHO_LIST) || security > SEC_PLAYER;

    // PLAYER see his team only and PLAYER can't see MODERATOR, GAME MASTER, ADMINISTRATOR characters
    // MODERATOR, GAME MASTER, ADMINISTRATOR can see all
    if( pFriend && pFriend->GetName() &&
        ( security > SEC_PLAYER ||
        ( pFriend->GetTeam() == team || allowTwoSideWhoList ) &&
        ( pFriend->GetSession()->GetSecurity() == SEC_PLAYER || gmInWhoList && pFriend->IsVisibleGloballyFor(player) )))
    {
        friendInfo.Status = FRIEND_STATUS_ONLINE;
        if(pFriend->isAFK())
            friendInfo.Status = FRIEND_STATUS_AFK;
        if(pFriend->isDND())
            friendInfo.Status = FRIEND_STATUS_DND;
        friendInfo.Area = pFriend->GetZoneId();
        friendInfo.Level = pFriend->getLevel();
        friendInfo.Class = pFriend->getClass();
    }
    else
    {
        friendInfo.Status = FRIEND_STATUS_OFFLINE;
        friendInfo.Area = 0;
        friendInfo.Level = 0;
        friendInfo.Class = 0;
    }
}

void SocialMgr::MakeFriendStatusPacket(FriendsResult result, uint32 guid, WorldPacket *data)
{
    data->Initialize(SMSG_FRIEND_STATUS, 5);
    *data << uint8(result);
    *data << uint64(guid);
}

void SocialMgr::SendFriendStatus(Player *player, FriendsResult result, uint32 friend_guid, std::string name, bool broadcast)
{
    FriendInfo fi;

    WorldPacket data;
    MakeFriendStatusPacket(result, friend_guid, &data);
    switch(result)
    {
        case FRIEND_ONLINE:
            GetFriendInfo(player, friend_guid, fi);
            data << uint8(fi.Status);
            data << uint32(fi.Area);
            data << uint32(fi.Level);
            data << uint32(fi.Class);
            break;
        case FRIEND_ADDED_ONLINE:
            GetFriendInfo(player, friend_guid, fi);
            data << name;
            data << uint8(fi.Status);
            data << uint32(fi.Area);
            data << uint32(fi.Level);
            data << uint32(fi.Class);
            break;
        case FRIEND_ADDED_OFFLINE:
            data << name;
            break;
    }

    if(broadcast)
        BroadcastToFriendListers(player, &data);
    else
        player->GetSession()->SendPacket(&data);
}

void SocialMgr::BroadcastToFriendListers(Player *player, WorldPacket *packet)
{
    if(!player)
        return;

    uint32 team     = player->GetTeam();
    uint32 security = player->GetSession()->GetSecurity();
    uint32 guid     = player->GetGUIDLow();
    bool gmInWhoList = sWorld.getConfig(CONFIG_GM_IN_WHO_LIST);
    bool allowTwoSideWhoList = sWorld.getConfig(CONFIG_ALLOW_TWO_SIDE_WHO_LIST);

    for(SocialMap::iterator itr = m_socialMap.begin(); itr != m_socialMap.end(); ++itr)
    {
        PlayerSocialMap::iterator itr2 = itr->second.m_playerSocialMap.find(guid);
        if(itr2 != itr->second.m_playerSocialMap.end())
        {
            Player *pFriend = ObjectAccessor::FindPlayer(MAKE_NEW_GUID(itr->first, 0, HIGHGUID_PLAYER));

            // PLAYER see his team only and PLAYER can't see MODERATOR, GAME MASTER, ADMINISTRATOR characters
            // MODERATOR, GAME MASTER, ADMINISTRATOR can see all
            if( pFriend && pFriend->IsInWorld() &&
                ( pFriend->GetSession()->GetSecurity() > SEC_PLAYER ||
                ( pFriend->GetTeam() == team || allowTwoSideWhoList ) &&
                (security == SEC_PLAYER || gmInWhoList && player->IsVisibleGloballyFor(pFriend) )))
            {
                pFriend->GetSession()->SendPacket(packet);
            }
        }
    }
}

PlayerSocial *SocialMgr::LoadFromDB(QueryResult *result, uint32 guid)
{
    PlayerSocial *social = &m_socialMap[guid];
    social->SetPlayerGUID(guid);

    if(!result)
        return social;

    uint32 friend_guid = 0;
    uint32 flags = 0;
    std::string note = "";

    do
    {
        Field *fields  = result->Fetch();

        friend_guid = fields[0].GetUInt32();
        flags = fields[1].GetUInt32();
        note = fields[2].GetCppString();

        social->m_playerSocialMap[friend_guid] = FriendInfo(flags, note);

        // prevent list (client-side) overflow
        if(social->m_playerSocialMap.size() >= 255)
            break;
    }
    while( result->NextRow() );
    delete result;
    return social;
}