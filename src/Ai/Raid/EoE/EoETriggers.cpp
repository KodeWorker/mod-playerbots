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
    // Ranged DPS only -- melee needs to stay in melee range of Malygos, not wander off to a
    // buff zone that's often clear across the room from the spark's spawn point.
    if (!botAI->IsRangedDps(bot)) { return false; }
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
    // Only exclude a bot actually airborne on a Hover Disk -- ground-targeted movement
    // wouldn't work for a vehicle passenger anyway, and the guide describes Deep Breath as
    // hitting "ground players" specifically, so a disk rider should already be safe.
    // Ground-based melee (still fighting the Nexus Lord, or waiting for a disk) need shelter
    // just like everyone else -- reported live: bots kept chasing the Scion objective through
    // an incoming Deep Breath instead of taking cover, so this also takes priority over
    // "malygos position" and "eoe hover disk" (see EoEStrategy.cpp).
    if (bot->GetVehicle()) { return false; }
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

bool HoverDiskCombatTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_HOVER_DISK);
}

bool HoverDiskTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss) { return false; }

    uint8 phase = MalygosTrigger::getPhase(bot, boss);
    if (phase != 2) { return false; }
    if (bot->GetVehicle()) { return false; }
    // Melee DPS only, per the strategy guide -- a disk's seat only frees up once its Nexus
    // Lord/Scion pilot dies, and the point is reaching the airborne Scion of Eternity that
    // melee otherwise can't touch. Tank stays grounded on its target; ranged/heal don't need
    // this at all.
    if (!botAI->IsMelee(bot) || botAI->IsTank(bot)) { return false; }

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
