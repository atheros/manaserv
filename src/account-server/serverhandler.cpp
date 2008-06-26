/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana World is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana World; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id$
 */

#include <cassert>
#include <sstream>
#include <list>

#include "account-server/serverhandler.hpp"

#include "account-server/accountclient.hpp"
#include "account-server/accounthandler.hpp"
#include "account-server/character.hpp"
#include "account-server/dalstorage.hpp"
#include "net/connectionhandler.hpp"
#include "net/messagein.hpp"
#include "net/messageout.hpp"
#include "net/netcomputer.hpp"
#include "serialize/characterdata.hpp"
#include "utils/logger.h"
#include "utils/tokendispenser.hpp"

struct MapStatistics
{
  std::vector< int > players;
  unsigned short nbThings;
  unsigned short nbMonsters;
};

typedef std::map< unsigned short, MapStatistics > ServerStatistics;

/**
 * Stores address, maps, and statistics, of a connected game server.
 */
struct GameServer: NetComputer
{
    GameServer(ENetPeer *peer): NetComputer(peer), port(0) {}

    std::string address;
    NetComputer *server;
    ServerStatistics maps;
    short port;
};

static GameServer *getGameServerFromMap(int);

/**
 * Manages communications with all the game servers.
 */
class ServerHandler: public ConnectionHandler
{
    friend GameServer *getGameServerFromMap(int);
    friend void GameServerHandler::dumpStatistics(std::ostream &);

    protected:
        /**
         * Processes server messages.
         */
        void processMessage(NetComputer *computer, MessageIn &message);

        /**
         * Called when a game server connects. Initializes a simple NetComputer
         * as these connections are stateless.
         */
        NetComputer *computerConnected(ENetPeer *peer);

        /**
         * Called when a game server disconnects.
         */
        void computerDisconnected(NetComputer *comp);
};

static ServerHandler *serverHandler;

bool GameServerHandler::initialize(int port)
{
    serverHandler = new ServerHandler;
    LOG_INFO("Game server handler started:");
    return serverHandler->startListen(port);
}

void GameServerHandler::deinitialize()
{
    serverHandler->stopListen();
    delete serverHandler;
}

void GameServerHandler::process()
{
    serverHandler->process(50);
}

NetComputer *ServerHandler::computerConnected(ENetPeer *peer)
{
    return new GameServer(peer);
}

void ServerHandler::computerDisconnected(NetComputer *comp)
{
    delete comp;
}

static GameServer *getGameServerFromMap(int mapId)
{
    for (ServerHandler::NetComputers::const_iterator
         i = serverHandler->clients.begin(),
         i_end = serverHandler->clients.end(); i != i_end; ++i)
    {
        GameServer *server = static_cast< GameServer * >(*i);
        ServerStatistics::const_iterator i = server->maps.find(mapId);
        if (i == server->maps.end()) continue;
        return server;
    }
    return NULL;
}

bool GameServerHandler::getGameServerFromMap
    (int mapId, std::string &address, int &port)
{
    if (GameServer *s = ::getGameServerFromMap(mapId))
    {
        address = s->address;
        port = s->port;
        return true;
    }
    return false;
}

static void registerGameClient
    (GameServer *s, std::string const &token, Character *ptr)
{
    MessageOut msg(AGMSG_PLAYER_ENTER);
    msg.writeString(token, MAGIC_TOKEN_LENGTH);
    msg.writeLong(ptr->getDatabaseID());
    msg.writeString(ptr->getName());
    serializeCharacterData(*ptr, msg);
    s->send(msg);
}

void GameServerHandler::registerClient(std::string const &token, Character *ptr)
{
    GameServer *s = ::getGameServerFromMap(ptr->getMapId());
    assert(s);
    registerGameClient(s, token, ptr);
}

void ServerHandler::processMessage(NetComputer *comp, MessageIn &msg)
{
    MessageOut result;
    GameServer *server = static_cast< GameServer * >(comp);

    switch (msg.getId())
    {
        case GAMSG_REGISTER:
        {
            LOG_DEBUG("GAMSG_REGISTER");
            // TODO: check the credentials of the game server
            server->address = msg.readString();
            server->port = msg.readShort();
            LOG_INFO("Game server " << server->address << ':' << server->port
                     << " wants to register " << (msg.getUnreadLength() / 2)
                     << " maps.");

            while (msg.getUnreadLength())
            {
                int id = msg.readShort();
                LOG_INFO("Registering map " << id << '.');
                if (GameServer *s = getGameServerFromMap(id))
                {
                    LOG_ERROR("Server Handler: map is already registered by "
                              << s->address << ':' << s->port << '.');
                }
                else
                {
                    MessageOut outMsg(AGMSG_ACTIVE_MAP);
                    outMsg.writeShort(id);
                    comp->send(outMsg);
                    MapStatistics &m = server->maps[id];
                    m.nbThings = 0;
                    m.nbMonsters = 0;
                }
            }
        } break;

        case GAMSG_PLAYER_DATA:
        {
            LOG_DEBUG("GAMSG_PLAYER_DATA");
            int id = msg.readLong();
            if (Character *ptr = storage->getCharacter(id, NULL))
            {
                deserializeCharacterData(*ptr, msg);
                if (!storage->updateCharacter(ptr))
                {
                    LOG_ERROR("Failed to update character "
                              << id << '.');
                }
                delete ptr;
            }
            else
            {
                LOG_ERROR("Received data for non-existing character "
                          << id << '.');
            }
        } break;

        case GAMSG_REDIRECT:
        {
            LOG_DEBUG("GAMSG_REDIRECT");
            int id = msg.readLong();
            std::string magic_token(utils::getMagicToken());
            if (Character *ptr = storage->getCharacter(id, NULL))
            {
                int mapId = ptr->getMapId();
                if (GameServer *s = getGameServerFromMap(mapId))
                {
                    registerGameClient(s, magic_token, ptr);
                    result.writeShort(AGMSG_REDIRECT_RESPONSE);
                    result.writeLong(id);
                    result.writeString(magic_token, MAGIC_TOKEN_LENGTH);
                    result.writeString(s->address);
                    result.writeShort(s->port);
                }
                else
                {
                    LOG_ERROR("Server Change: No game server for map " <<
                              mapId << '.');
                }
                delete ptr;
            }
            else
            {
                LOG_ERROR("Received data for non-existing character "
                          << id << '.');
            }
        } break;

        case GAMSG_PLAYER_RECONNECT:
        {
            LOG_DEBUG("GAMSG_PLAYER_RECONNECT");
            int id = msg.readLong();
            std::string magic_token = msg.readString(MAGIC_TOKEN_LENGTH);

            if (Character *ptr = storage->getCharacter(id, NULL))
            {
                int accountID = ptr->getAccountID();
                AccountClientHandler::prepareReconnect(magic_token, accountID);
                delete ptr;
            }
            else
            {
                LOG_ERROR("Received data for non-existing character "
                          << id << '.');
            }
        } break;

        case GAMSG_GET_QUEST:
        {
            int id = msg.readLong();
            std::string name = msg.readString();
            std::string value = storage->getQuestVar(id, name);
            result.writeShort(AGMSG_GET_QUEST_RESPONSE);
            result.writeLong(id);
            result.writeString(name);
            result.writeString(value);
        } break;

        case GAMSG_SET_QUEST:
        {
            int id = msg.readLong();
            std::string name = msg.readString();
            std::string value = msg.readString();
            storage->setQuestVar(id, name, value);
        } break;

        case GAMSG_BAN_PLAYER:
        {
            int id = msg.readLong();
            int duration = msg.readShort();
            storage->banCharacter(id, duration);
        } break;

        case GAMSG_STATISTICS:
        {
            while (msg.getUnreadLength())
            {
                int mapId = msg.readShort();
                ServerStatistics::iterator i = server->maps.find(mapId);
                if (i == server->maps.end())
                {
                    LOG_ERROR("Server " << server->address << ':'
                              << server->port << " should not be sending stati"
                              "stics for map " << mapId << '.');
                    // Skip remaining data.
                    break;
                }
                MapStatistics &m = i->second;
                m.nbThings = msg.readShort();
                m.nbMonsters = msg.readShort();
                int nb = msg.readShort();
                m.players.resize(nb);
                for (int j = 0; j < nb; ++j)
                {
                    m.players[j] = msg.readLong();
                }
            }
        } break;

#if 0
        case GAMSG_GUILD_CREATE:
        {
            LOG_DEBUG("GAMSG_GUILD_CREATE");

            result.writeShort(AGMSG_GUILD_CREATE_RESPONSE);
            // Check if the guild name is taken already
            int playerId = msg.readLong();
            std::string guildName = msg.readString();
            if (guildManager->findByName(guildName) != NULL)
            {
                result.writeByte(ERRMSG_ALREADY_TAKEN);
                break;
            }
            result.writeByte(ERRMSG_OK);

            Storage &store = Storage::instance("tmw");
            CharacterPtr ptr = store.getCharacter(playerId);

            // Add guild to character data.
            ptr->addGuild(guildName);

            // Who to send data to at the other end
            result.writeLong(playerId);

            short guildId = guildManager->createGuild(guildName, ptr.get());
            result.writeShort(guildId);
            result.writeString(guildName);
            result.writeShort(1);
            enterChannel(guildName, ptr.get());
        } break;

        case GAMSG_GUILD_INVITE:
        {
            // Add Inviting member to guild here
            LOG_DEBUG("Received msg ... GAMSG_GUILD_INVITE");
            result.writeShort(AGMSG_GUILD_INVITE_RESPONSE);
            // Check if user can invite users
            int playerId = msg.readLong();
            short id = msg.readShort();
            std::string member = msg.readString();
            Guild *guild = guildManager->findById(id);

            Storage &store = Storage::instance("tmw");
            CharacterPtr ptr = store.getCharacter(playerId);

            if (!guild->checkLeader(ptr.get()))
            {
                // Return that the user doesnt have the rights to invite.
                result.writeByte(ERRMSG_INSUFFICIENT_RIGHTS);
                break;
            }

            if (guild->checkInGuild(member))
            {
                // Return that invited member already in guild.
                result.writeByte(ERRMSG_ALREADY_TAKEN);
                break;
            }

            // Send invite to player using chat server
            if (store.doesCharacterNameExist(member))
            {
                sendInvite(member, ptr->getName(), guild->getName());
            }

            guild->addInvited(member);
            result.writeByte(ERRMSG_OK);
        } break;

        case GAMSG_GUILD_ACCEPT:
        {
            // Add accepting into guild
            LOG_DEBUG("Received msg ... GAMSG_GUILD_ACCEPT");
            result.writeShort(AGMSG_GUILD_ACCEPT_RESPONSE);
            int playerId = msg.readLong();
            std::string guildName = msg.readString();
            Guild *guild = guildManager->findByName(guildName);
            if (!guild)
            {
                // Return the guild does not exist.
                result.writeByte(ERRMSG_INVALID_ARGUMENT);
                break;
            }

            Storage &store = Storage::instance("tmw");
            CharacterPtr ptr = store.getCharacter(playerId);

            if (!guild->checkInvited(ptr->getName()))
            {
                // Return the user was not invited.
                result.writeByte(ERRMSG_INSUFFICIENT_RIGHTS);
                break;
            }

            if (guild->checkInGuild(ptr->getName()))
            {
                // Return that the player is already in the guild.
                result.writeByte(ERRMSG_ALREADY_TAKEN);
                break;
            }

            result.writeByte(ERRMSG_OK);

            // Who to send data to at the other end
            result.writeLong(playerId);

            // The guild id and guild name they have joined
            result.writeShort(guild->getId());
            result.writeString(guildName);

            // Add member to guild
            guildManager->addGuildMember(guild->getId(), ptr.get());

            // Add guild to character
            ptr->addGuild(guildName);

            // Enter Guild Channel
            enterChannel(guildName, ptr.get());
        } break;

        case GAMSG_GUILD_GET_MEMBERS:
        {
            LOG_DEBUG("Received msg ... GAMSG_GUILD_GET_MEMBERS");
            result.writeShort(AGMSG_GUILD_GET_MEMBERS_RESPONSE);
            int playerId = msg.readLong();
            short guildId = msg.readShort();
            Guild *guild = guildManager->findById(guildId);
            if (!guild)
            {
                result.writeByte(ERRMSG_INVALID_ARGUMENT);
                break;
            }
            result.writeByte(ERRMSG_OK);
            result.writeLong(playerId);
            result.writeShort(guildId);
            for (std::list<std::string>::const_iterater itr = guild->getMembers()->begin(); 
                 itr != guild->getMembers()->end(); ++itr)
            {
                result.writeString((*itr));
            }
        } break;

        case GAMSG_GUILD_QUIT:
        {
            LOG_DEBUG("Received msg ... GAMSG_GUILD_QUIT");
            result.writeShort(AGMSG_GUILD_QUIT_RESPONSE);
            int playerId = msg.readLong();
            short guildId = msg.readShort();
            Guild *guild = guildManager->findById(guildId);
            if (!guild)
            {
                result.writeByte(ERRMSG_INVALID_ARGUMENT);
                break;
            }
            Storage &store = Storage::instance("tmw");
            CharacterPtr ptr = store.getCharacter(playerId);
            guildManager->removeGuildMember(guildId, ptr.get());
            result.writeByte(ERRMSG_OK);
            result.writeLong(playerId);
            result.writeShort(guildId);
        } break;
#endif

        default:
            LOG_WARN("ServerHandler::processMessage, Invalid message type: "
                     << msg.getId());
            result.writeShort(XXMSG_INVALID);
            break;
    }

    // return result
    if (result.getLength() > 0)
        comp->send(result);
}

void GameServerHandler::dumpStatistics(std::ostream &os)
{
    for (ServerHandler::NetComputers::const_iterator
         i = serverHandler->clients.begin(),
         i_end = serverHandler->clients.end(); i != i_end; ++i)
    {
        GameServer *server = static_cast< GameServer * >(*i);
        if (!server->port) continue;
        os << "<gameserver address=\"" << server->address << "\" port=\""
           << server->port << "\">\n";

        for (ServerStatistics::const_iterator j = server->maps.begin(),
             j_end = server->maps.end(); j != j_end; ++j)
        {
            MapStatistics const &m = j->second;
            os << "<map id=\"" << j->first << "\" nb_things=\"" << m.nbThings
               << "\" nb_monsters=\"" << m.nbMonsters << "\">\n";
            for (std::vector< int >::const_iterator k = m.players.begin(),
                 k_end = m.players.end(); k != k_end; ++k)
            {
                os << "<character id=\"" << *k << "\"/>\n";
            }
            os << "</map>\n";
        }
        os << "</gameserver>\n";
    }
}

#if 0
void ServerHandler::enterChannel(const std::string &name,
                                 CharacterData *player)
{
    MessageOut result(CPMSG_ENTER_CHANNEL_RESPONSE);

    short channelId = chatChannelManager->getChannelId(name);
    ChatChannel *channel = chatChannelManager->getChannel(channelId);

    if (!channel)
    {
        // Channel doesn't exist yet so create one
        channelId = chatChannelManager->registerPrivateChannel(name,
                                                               "Guild Channel",
                                                               "");
        channel = chatChannelManager->getChannel(channelId);
    }

    if (channel && channel->addUser(player->getName()))
    {
        result.writeByte(ERRMSG_OK);

        // The user entered the channel, now give him the channel id, the
        // announcement string and the user list.
        result.writeShort(channelId);
        result.writeString(name);
        result.writeString(channel->getAnnouncement());
        const ChatChannel::ChannelUsers &userList = channel->getUserList();

        for (ChatChannel::ChannelUsers::const_iterator i = userList.begin(),
                i_end = userList.end();
                i != i_end; ++i)
        {
            result.writeString(*i);
        }

        // Send an CPMSG_UPDATE_CHANNEL to warn other clients a user went
        // in the channel.
        chatHandler->warnUsersAboutPlayerEventInChat(channel,
                                                     player->getName(),
                                                     CHAT_EVENT_NEW_PLAYER);

    }

    chatHandler->sendGuildEnterChannel(result, player->getName());
}

void ServerHandler::sendInvite(const std::string &invitedName,
                               const std::string &inviterName,
                               const std::string &guildName)
{
    // TODO: Separate account and chat server
    chatHandler->sendGuildInvite(invitedName, inviterName, guildName);
}
#endif
