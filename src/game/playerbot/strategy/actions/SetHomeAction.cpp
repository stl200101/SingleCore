#include "../../../pchdef.h"
#include "../../playerbot.h"
#include "SetHomeAction.h"
#include "../../PlayerbotAIConfig.h"


using namespace ai;

bool SetHomeAction::Execute(Event event)
{
    Player* master = ai->GetMaster();
    if (!master)
        return false;

    ObjectGuid selection = master->GetSelectionGuid();
    if (selection)
    {
        MapPtr map = bot->GetMapPtr();
        Unit* unit = map->GetUnit(selection);
        map = MapPtr();
        if (unit && unit->isInnkeeper())
        {
            float angle = GetFollowAngle();
            float x = unit->GetPositionX() + sPlayerbotAIConfig.followDistance * cos(angle);
            float y = unit->GetPositionY() + sPlayerbotAIConfig.followDistance * sin(angle);
            float z = unit->GetPositionZ();
            WorldLocation loc(unit->GetMapId(), x, y, z);
            bot->SetHomebindToLocation(loc);
            ai->TellMaster("This inn is my new home");
            return true;
        }
    }

    list<ObjectGuid> npcs = AI_VALUE(list<ObjectGuid>, "nearest npcs");
    for (list<ObjectGuid>::iterator i = npcs.begin(); i != npcs.end(); i++)
    {
        Creature *unit = bot->GetNPCIfCanInteractWith(*i, UNIT_NPC_FLAG_INNKEEPER);
        if (!unit)
            continue;

        bot->GetSession()->SendBindPoint(unit);
        ai->TellMaster("This inn is my new home");
        return true;
    }

    ai->TellMaster("Can't find any innkeeper around");
    return false;
}
