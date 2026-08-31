#include "EoETriggers.h"

#include "EoEActions.h"
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
            // Sparks spawn at the room's four corners (FourSidesPos in boss_malygos.cpp),
            // often 90+ yards from Malygos -- well past normal ranged spell range. Chasing
            // one that far means abandoning the fight for a 180+ yard round trip, only to
            // get pulled straight back to combat range by normal positioning the instant
            // the buff lands -- reported live as "quickly move away". Only worth it if
            // reasonably close already.
            float maxChaseDistance = 50.0f;
            if (bot->GetDistance2d(unit->GetPositionX(), unit->GetPositionY()) > maxChaseDistance)
            {
                continue;
            }
            // Also skip if it's too far from the boss itself (not just from this bot) -- not
            // worth it if it'd sit outside spell range regardless of who goes for it -- or if
            // it's too close to the tank position (Malygos's front/head), where Arcane
            // Breath's cone punishes anyone standing there who isn't the tank. Requested live.
            if (boss->GetDistance2d(unit->GetPositionX(), unit->GetPositionY()) > maxChaseDistance)
            {
                continue;
            }
            float minTankDistance = 15.0f;
            if (unit->GetDistance2d(MALYGOS_MAINTANK_POSITION.first, MALYGOS_MAINTANK_POSITION.second) < minTankDistance)
            {
                continue;
            }
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
    // Only exclude a bot actually riding a vehicle (disk or otherwise) -- ground-targeted
    // movement wouldn't work for a vehicle passenger anyway, and per the guide Deep Breath
    // hits "ground players" specifically, so a disk rider should already be safe. Confirmed
    // live: disk-riding melee should attack only, never diverted for breath/bubble (covered
    // by this check) -- but ground-based melee (still fighting the Nexus Lord, or between
    // disks) correctly still takes shelter, and only during the actual breath windup thanks
    // to the position gate below, not constantly.
    if (bot->GetVehicle()) { return false; }
    // Healers take shelter too, per the strategy guide and confirmed live -- everyone not on
    // a disk should hide in the bubble during Deep Breath. The earlier attempt to fix a
    // healer death by excluding healers outright was the wrong lever: the real problem is
    // that a hard-cast heal fails outright while the healer is physically moving there, which
    // instant-cast heals don't suffer from (see EoEActions.h's CastDrakeSpellAction-style
    // cast-time check, generic version). The class-level heal-priority system already tries
    // instant options first when they're actually off cooldown -- confirmed live that a death
    // happened specifically when a healer's instant options (Swiftmend/Wild Growth) were both
    // still on cooldown at that exact moment, leaving only hard-casts, which then failed to
    // movement. That's a real cooldown-availability gap, not something to paper over by
    // pulling healers out of the danger zone instead.

    // Gate on the Deep Breath/Surge of Power windup specifically, instead of reacting any
    // time the buff happens to be missing -- reported live ("bots did not go bubble when
    // Malygos shouted"; the previous always-on version was also implicated in dragging
    // Phase 2 out by pulling bots off their kill target too often). This project has no
    // chat/yell-listening infrastructure anywhere to react to the actual shout
    // (SAY_DEEP_BREATH), so the closest reliable proxy is boss position: Malygos moves to
    // within ~10yd of the room's center and goes idle right before it lands
    // (EVENT_MOVE_TO_SURGE_OF_POWER in boss_malygos.cpp) -- normal Phase 2 patrol circles at
    // a much larger radius, so this shouldn't false-positive during ordinary movement.
    //
    // Deliberately NOT gated on already having the protection buff: this trigger needs to
    // stay active (and its action needs to hold position, see ArcaneOverloadBubbleAction)
    // for the bot's *entire* time inside the danger window, or a lower-priority action like
    // "malygos position" reclaims the movement slot the instant the buff lands and walks the
    // bot back out mid-breath, dropping the (proximity-based) aura before it landed --
    // reported live ("hide in bubble should stay until the breath ends"). Control only
    // releases once Malygos actually leaves center.
    float breathWarningRadius = 20.0f;
    if (boss->GetDistance2d(MALYGOS_CENTER_POSITION.first, MALYGOS_CENTER_POSITION.second) > breathWarningRadius)
    {
        return false;
    }

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
    // Melee DPS gets first crack, per the strategy guide -- a disk's seat only frees up once
    // its Nexus Lord/Scion pilot dies, and the point is reaching the airborne Scion that melee
    // otherwise can't touch. Opened to ranged DPS and tanks too (requested live) so a freed
    // seat doesn't go to waste once melee doesn't need it -- healers still excluded, they
    // should keep healing rather than go DPS-flying.
    if (botAI->IsHeal(bot)) { return false; }

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
