#include "EoETriggers.h"

#include "SharedDefines.h"
#include "Vehicle.h"

uint8 MalygosTrigger::getPhase(Player* bot, Unit* boss)
{
    uint8 phase = 0;
    Unit* vehicle = bot->GetVehicleBase();
    if (bot->GetMapId() != EOE_MAP_ID) { return phase; }

    if (vehicle && vehicle->GetEntry() == NPC_WYRMREST_SKYTALON)
    {
        phase = 3;
    }
    else if (boss && boss->HealthAbovePct(50))
    {
        phase = 1;
    }
    else if (boss)
    {
        phase = 2;
    }

    return phase;
}

bool MalygosTrigger::IsActive()
{
    return bool(AI_VALUE2(Unit*, "find target", "malygos"));
}

bool PowerSparkTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss) { return false; }

    if (bot->getClass() != CLASS_DEATH_KNIGHT)
    {
        return false;
    }

    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && unit->GetEntry() == NPC_POWER_SPARK)
        {
            return true;
        }
    }

    return false;
}

bool PowerSparkGroundBuffTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss) { return false; }

    uint8 phase = MalygosTrigger::getPhase(bot, boss);
    if (phase != 1) { return false; }
    if (botAI->IsTank(bot)) { return false; }  // stay on the boss, don't chase the buff zone
    if (bot->HasAura(SPELL_POWER_SPARK_GROUND_BUFF)) { return false; }

    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    LOG_DEBUG("playerbots", "[EoE debug] {} power spark buff scan: phase={} candidates={}",
        bot->GetName(), phase, targets.size());
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit) { continue; }
        if (unit->GetEntry() == NPC_POWER_SPARK)
        {
            LOG_DEBUG("playerbots", "[EoE debug] {} found spark {} alive={} flags={}",
                bot->GetName(), unit->GetGUID().ToString(), unit->IsAlive(), unit->GetUnitFlags());
        }
        if (unit->GetEntry() == NPC_POWER_SPARK && unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
        {
            return true;
        }
    }

    return false;
}

bool ArcaneOverloadBubbleTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss) { return false; }

    uint8 phase = MalygosTrigger::getPhase(bot, boss);
    if (phase != 2) { return false; }
    if (botAI->IsTank(bot)) { return false; }  // stay on the Nexus Lord/Scion, don't chase the shield
    if (bot->HasAura(SPELL_ARCANE_OVERLOAD_PROTECTION)) { return false; }

    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    LOG_DEBUG("playerbots", "[EoE debug] {} arcane overload scan: phase={} candidates={}",
        bot->GetName(), phase, targets.size());
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit) { continue; }
        if (unit->GetEntry() == NPC_ARCANE_OVERLOAD)
        {
            LOG_DEBUG("playerbots", "[EoE debug] {} found overload {} alive={} flags={}",
                bot->GetName(), unit->GetGUID().ToString(), unit->IsAlive(), unit->GetUnitFlags());
            return true;
        }
    }

    return false;
}

bool HoverDiskTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss) { return false; }

    uint8 phase = MalygosTrigger::getPhase(bot, boss);
    if (phase != 2) { return false; }
    if (bot->GetVehicle()) { return false; }
    if (botAI->IsTank(bot)) { return false; }  // stay on the ground tanking, don't board the disk

    GuidVector targets = AI_VALUE(GuidVector, "nearest vehicles");
    LOG_DEBUG("playerbots", "[EoE debug] {} hover disk scan: phase={} candidates={}",
        bot->GetName(), phase, targets.size());
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit) { continue; }
        if (unit->GetEntry() == NPC_HOVER_DISK)
        {
            Vehicle* veh = unit->GetVehicleKit();
            LOG_DEBUG("playerbots", "[EoE debug] {} found disk {} alive={} hasVehicleKit={} seats={}",
                bot->GetName(), unit->GetGUID().ToString(), unit->IsAlive(), bool(veh),
                veh ? (int)veh->GetAvailableSeatCount() : -1);
            if (veh && veh->GetAvailableSeatCount())
            {
                return true;
            }
        }
    }

    return false;
}
