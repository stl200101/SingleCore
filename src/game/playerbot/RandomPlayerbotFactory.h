#ifndef _RandomPlayerbotFactory_H
#define _RandomPlayerbotFactory_H

#include "Common.h"
#include "PlayerbotAIBase.h"

class WorldPacket;
class Player;
class Unit;
class Object;
class Item;

using namespace std;

class MANGOS_DLL_SPEC RandomPlayerbotFactory
{
    public:
        RandomPlayerbotFactory(uint32 accountId);
		virtual ~RandomPlayerbotFactory() {}

	public:
        bool CreateRandomBot(uint8 cls);

	private:
        string CreateRandomBotName();

    private:
        uint32 accountId;
        static map<uint8, vector<uint8> > availableRaces;
};

#endif
