#include "Playerbots.h"
#include "EoEActions.h"
#include "EoETriggers.h"
#include "Vehicle.h"

namespace
{
    // Shared between MalygosTargetAction and EoEHoverDiskAttackAction, keyed by rider, so a
    // disk rider's attack target and flight target always agree on which Scion to engage --
    // picking independently let a rider fly toward one Scion while shooting at another.
    std::unordered_map<ObjectGuid, ObjectGuid> sDiskRiderScion;

    Unit* GetOrPickRiderScion(PlayerbotAI* botAI, Player* bot)
    {
        ObjectGuid botGuid = bot->GetGUID();
        auto it = sDiskRiderScion.find(botGuid);
        Unit* scion = (it != sDiskRiderScion.end()) ? botAI->GetUnit(it->second) : nullptr;
        if (!scion || !scion->IsInWorld() || !scion->IsAlive() || scion->GetEntry() != NPC_SCION_OF_ETERNITY)
        {
            scion = nullptr;
            // Not using the AI_VALUE macro here -- it expands against a "context" member that
            // only exists inside Action subclasses, and this is a free function.
            GuidVector targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
            for (auto& target : targets)
            {
                Unit* unit = botAI->GetUnit(target);
                if (unit && unit->GetEntry() == NPC_SCION_OF_ETERNITY)
                {
                    scion = unit;
                    sDiskRiderScion[botGuid] = unit->GetGUID();
                    break;
                }
            }
        }
        return scion;
    }
}

bool MalygosPositionAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss) { return false; }

    uint8 phase = MalygosTrigger::getPhase(bot, boss);

    float distance = 5.0f;

    if (phase == 1)
    {
        Unit* spark = nullptr;

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (unit && unit->GetEntry() == NPC_POWER_SPARK)
            {
                spark = unit;
                break;
            }
        }

        // Position tank
        if (botAI->IsMainTank(bot))
        {
            if (bot->GetDistance2d(MALYGOS_MAINTANK_POSITION.first, MALYGOS_MAINTANK_POSITION.second) > distance)
            {
                return MoveTo(EOE_MAP_ID, MALYGOS_MAINTANK_POSITION.first, MALYGOS_MAINTANK_POSITION.second, bot->GetPositionZ(),
                    false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }
            return false;
        }
        // Position DK for spark pull
        // else if (spark && bot->IsClass(CLASS_DEATH_KNIGHT))
        // {
        //     if (bot->GetDistance2d(MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second) > distance)
        //     {
        //         bot->Yell("SPARK SPAWNED, MOVING TO STACK", LANG_UNIVERSAL);
        //         return MoveTo(EOE_MAP_ID, MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second, bot->GetPositionZ(),
        //             false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        //     }
        //     return false;
        // }
        else if (spark)
        {
            return false;
        }
        else if (!bot->IsClass(CLASS_HUNTER))
        {
            if (bot->GetDistance2d(MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second) > (distance * 3.0f))
            {
                return MoveTo(EOE_MAP_ID, MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second, bot->GetPositionZ(),
                    false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
            }
            return false;
        }
    }
    else if (phase == 2)
    {
        // Tank has its own positioning via ArcaneOverloadBubbleTrigger; ranged/heal don't need
        // to close melee distance at all.
        if (botAI->IsTank(bot) || !botAI->IsMelee(bot)) { return false; }

        Unit* addTarget = nullptr;
        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (unit && (unit->GetEntry() == NPC_NEXUS_LORD || unit->GetEntry() == NPC_SCION_OF_ETERNITY))
            {
                addTarget = unit;
                break;
            }
        }
        if (!addTarget) { return false; }

        // The add is unreachable by ground pathfinding while still airborne on its Hover
        // Disk intro flight -- move toward its (x,y) so melee is in place the instant it lands.
        if (bot->GetDistance2d(addTarget->GetPositionX(), addTarget->GetPositionY()) > distance)
        {
            return MoveTo(EOE_MAP_ID, addTarget->GetPositionX(), addTarget->GetPositionY(), bot->GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
        return false;
    }

    return false;
}

bool MalygosTargetAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss) { return false; }

    uint8 phase = MalygosTrigger::getPhase(bot, boss);

    if (phase == 1)
    {
        if (botAI->IsHeal(bot)) { return false; }

        // Init this as boss by default, if no better target is found just fall back to Malygos
        Unit* newTarget = boss;
        Unit* spark = nullptr;

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            // Skip a spark that's already been killed (ground-buff state, non-attackable) --
            // only chase one still alive and heading for the boss.
            if (unit && unit->GetEntry() == NPC_POWER_SPARK && !unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
            {
                spark = unit;
                break;
            }
        }

        // Ranged DPS kill the spark before it reaches Malygos -- otherwise it buffs the boss
        // (SPELL_POWER_SPARK_MALYGOS_BUFF); killing it also leaves a ground buff for the raid
        // (see PowerSparkGroundBuffTrigger).
        if (spark && botAI->IsRangedDps(bot))
        {
            newTarget = spark;
        }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        if (!currentTarget || currentTarget->GetEntry() != newTarget->GetEntry())
        {
            return Attack(newTarget);
        }
    }
    else if (phase == 2)
    {
        if (botAI->IsHeal(bot)) { return false; }

        Unit* newTarget = nullptr;
        Unit* nexusLord = nullptr;
        Unit* scionOfEternity = nullptr;

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (!unit) { continue; }

            if (unit->GetEntry() == NPC_NEXUS_LORD)
            {
                nexusLord = unit;
            }
            else if (unit->GetEntry() == NPC_SCION_OF_ETERNITY)
            {
                scionOfEternity = unit;
            }
        }

        // A disk rider's Nexus Lord already died (that's why the seat was free), so it's
        // airborne specifically to reach the Scion -- target that regardless of role.
        Unit* diskVehicle = bot->GetVehicleBase();
        bool onHoverDisk = diskVehicle && diskVehicle->GetEntry() == NPC_HOVER_DISK;

        if (onHoverDisk)
        {
            // Never fall back to a grounded Lord even if one is still alive elsewhere --
            // it's out of the disk's reach (no melee, and its own Nexus Lord is already
            // dead), but LoS to it from the air usually succeeds, so without this a rider
            // would lock onto an unreachable Lord instead of the Scion it's flying toward.
            // Use the same Scion EoEHoverDiskAttackAction is flying toward, not a fresh
            // pick, so attack target and flight target never disagree.
            newTarget = GetOrPickRiderScion(botAI, bot);
        }
        // Focus the Nexus Lord first with everyone, only pivoting to the Scion once the Lord
        // is dead/not up, instead of splitting damage between both simultaneously. Grounded
        // tank/melee never fall back to the Scion -- it's airborne and unreachable to them,
        // and generic combat movement chasing it walks them right out of their bubble.
        else if (botAI->IsRangedDps(bot) && scionOfEternity && !nexusLord)
        {
            newTarget = scionOfEternity;
        }
        else if (nexusLord)
        {
            newTarget = nexusLord;
        }

        if (!newTarget) { return false; }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        if (!currentTarget || currentTarget->GetEntry() != newTarget->GetEntry())
        {
            return Attack(newTarget);
        }
    }

    // else if (phase == 3)
    // {}

    return false;
}

// bool PullPowerSparkAction::Execute(Event event)
// {
//     Unit* spark = nullptr;

//     GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
//     for (auto& target : targets)
//     {
//         Unit* unit = botAI->GetUnit(target);
//         if (unit && unit->GetEntry() == NPC_POWER_SPARK)
//         {
//             spark = unit;
//             break;
//         }
//     }

//     if (!spark) { return false; }

//     if (spark->GetDistance2d(MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second) > 3.0f)
//     {
//         bot->Yell("GRIPPING SPARK", LANG_UNIVERSAL);
//         return botAI->CastSpell("death grip", spark);
//     }

//     return false;
// }

// bool PullPowerSparkAction::isPossible()
// {
//     Unit* spark = nullptr;

//     GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
//     for (auto& target : targets)
//     {
//         Unit* unit = botAI->GetUnit(target);
//         if (unit && unit->GetEntry() == NPC_POWER_SPARK)
//         {
//             spark = unit;
//             break;
//         }
//     }

//     return botAI->CanCastSpell(spell, spark);
// }

// bool PullPowerSparkAction::isUseful()
// {
//     Unit* spark = nullptr;

//     GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
//     for (auto& target : targets)
//     {
//         Unit* unit = botAI->GetUnit(target);
//         if (unit && unit->GetEntry() == NPC_POWER_SPARK)
//         {
//             spark = unit;
//             break;
//         }
//     }

//     if (!spark)
//         return false;

//     if (!spark->IsInWorld() || spark->GetMapId() != bot->GetMapId())
//         return false;

//     return bot->GetDistance2d(MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second) < 3.0f;
// }

// bool KillPowerSparkAction::Execute(Event event)
// {
//     return false;
// }

bool EoEFlyDrakeAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}
bool EoEFlyDrakeAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();
    if (!master) { return false; }
    Unit* masterVehicle = master->GetVehicleBase();
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase || !masterVehicle) { return false; }

    MotionMaster* mm = vehicleBase->GetMotionMaster();

    // Marked for the incoming Surge of Power blast (25man only, see the constant's comment) --
    // break formation and fly away from the raid so the AoE doesn't catch everyone else too.
    if (vehicleBase->HasAura(SPELL_SURGE_OF_POWER_WARN_SELECTOR_25))
    {
        mm->Clear(false);
        float angle = vehicleBase->GetAngle(masterVehicle) + static_cast<float>(M_PI);
        float fleeDist = 25.0f;
        float x = vehicleBase->GetPositionX() + cos(angle) * fleeDist;
        float y = vehicleBase->GetPositionY() + std::sin(angle) * fleeDist;
        vehicleBase->SetCanFly(true);
        mm->MovePoint(0, x, y, vehicleBase->GetPositionZ());
        vehicleBase->SendMovementFlagUpdate();
        return true;
    }

    // Static Field is a stationary hazard dealing periodic damage for ~20s
    // (EVENT_SPELL_STATIC_FIELD in boss_malygos.cpp). The generic AvoidAoeAction repositions
    // the bot's own character, not the vehicle it's riding, so this has to be handled here.
    GuidVector nearbyNpcs = AI_VALUE(GuidVector, "nearest npcs");
    for (auto& npc : nearbyNpcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (!unit || unit->GetEntry() != NPC_STATIC_FIELD) { continue; }

        float dangerRadius = 15.0f;  // estimate -- radius isn't queryable from this DB
        if (vehicleBase->GetExactDist2d(unit) > dangerRadius) { continue; }

        mm->Clear(false);
        float angle = unit->GetAngle(vehicleBase);  // bearing from the hazard toward the bot
        float fleeDist = dangerRadius + 10.0f;
        float x = unit->GetPositionX() + cos(angle) * fleeDist;
        float y = unit->GetPositionY() + std::sin(angle) * fleeDist;
        vehicleBase->SetCanFly(true);
        mm->MovePoint(0, x, y, vehicleBase->GetPositionZ());
        vehicleBase->SendMovementFlagUpdate();
        return true;
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (boss && false)
    {
        // Handle as boss encounter instead of formation flight
        mm->Clear(false);
        float distance = vehicleBase->GetExactDist(boss);
        float range = 55.0f;    // Drake range is 60yd
        if (distance > range)
        {
            mm->MoveForwards(boss, range - distance);
            vehicleBase->SendMovementFlagUpdate();
            return true;
        }

        vehicleBase->SetFacingToObject(boss);
        mm->MoveIdle();
        vehicleBase->SendMovementFlagUpdate();
        return false;
    }

    if (vehicleBase->GetExactDist(masterVehicle) > 5.0f)
    {
        uint8 numPlayers;
        bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? numPlayers = 25 : numPlayers = 10;
        // 3/4 of a circle, with frontal cone 90 deg unobstructed
        float angle = botAI->GetGroupSlotIndex(bot) * (2*M_PI - M_PI_2)/numPlayers + M_PI_2;
        // float angle = M_PI;
        // Wide follow radius -- a tight ring packs the raid into one spot, letting a single
        // AoE (Surge of Power, Static Field) hit everyone at once.
        float followDist = 15.0f;

        // MovePoint to a computed absolute slot, not MoveFollow -- CanCastVehicleSpell()
        // rejects any spell with a cast time while vehicleBase->isMoving(), and MoveFollow
        // never truly stops (continuous chase, so isMoving() reads true nearly permanently).
        // MovePoint actually arrives and idles, giving real cast windows between reposition.
        float masterAngle = masterVehicle->GetOrientation();
        float slotX = masterVehicle->GetPositionX() + cos(masterAngle + angle) * followDist;
        float slotY = masterVehicle->GetPositionY() + std::sin(masterAngle + angle) * followDist;
        vehicleBase->SetCanFly(true);
        mm->MovePoint(0, slotX, slotY, masterVehicle->GetPositionZ());
        vehicleBase->SendMovementFlagUpdate();
        return true;
    }
    return false;
}

bool EoEDrakeAttackAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool EoEDrakeAttackAction::Execute(Event /*event*/)
{
    vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase)
    {
        return false;
    }

    // Keep Flame Shield up against Malygos's periodic Arcane Pulse / Surge of Power damage --
    // takes priority over both the DPS and heal rotations below.
    if (!vehicleBase->HasAura(SPELL_FLAME_SHIELD) && CastDrakeSpellAction(vehicleBase, SPELL_FLAME_SHIELD, 0))
    {
        return true;
    }

    // Unit* target = AI_VALUE(Unit*, "current target");
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    // if (!boss) { return false; }

    if (!boss)
    {
        GuidVector npcs = AI_VALUE(GuidVector, "possible targets");
        for (auto& npc : npcs)
        {
            Unit* unit = botAI->GetUnit(npc);
            if (!unit || unit->GetEntry() != NPC_MALYGOS)
            {
                continue;
            }

            boss = unit;
            break;
        }
    }
    // Check this again to see if a target was assigned
    if (!boss)
    {
        return false;
    }

    if (botAI->IsHeal(bot))
    {
        return DrakeHealAction();
    }
    return DrakeDpsAction(boss);
}

bool EoEDrakeAttackAction::CastDrakeSpellAction(Unit* target, uint32 spellId, uint32 cooldown)
{
    bool canCast = botAI->CanCastVehicleSpell(spellId, target);
    if (canCast)
    {
        if (botAI->CastVehicleSpell(spellId, target))
        {
            vehicleBase->AddSpellCooldown(spellId, 0, cooldown);
            LOG_DEBUG("playerbots", "[EoE debug] {} drake spell {} on {} -- CAST OK",
                bot->GetName(), spellId, target->GetName());
            return true;
        }
        LOG_DEBUG("playerbots", "[EoE debug] {} drake spell {} on {} -- CanCast true, Cast FAILED",
            bot->GetName(), spellId, target->GetName());
    }
    else
    {
        LOG_DEBUG("playerbots", "[EoE debug] {} drake spell {} on {} -- CanCast FALSE",
            bot->GetName(), spellId, target->GetName());
    }
    return false;
}

bool EoEDrakeAttackAction::DrakeDpsAction(Unit* target)
{
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase) { return false; }

    uint8 comboPoints = vehicleBase->GetComboPoints(target);
    if (comboPoints >= 2)
    {
        return CastDrakeSpellAction(target, SPELL_ENGULF_IN_FLAMES, 0);
    }
    else
    {
        return CastDrakeSpellAction(target, SPELL_FLAME_SPIKE, 0);
    }
}

bool EoEDrakeAttackAction::DrakeHealAction()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase)
    {
        return false;
    }

    uint8 comboPoints = vehicleBase->GetComboPoints(vehicleBase);
    if (comboPoints >= 5)
    {
        return CastDrakeSpellAction(vehicleBase, SPELL_LIFE_BURST, 0);
    }
    else
    {
        // "Revivify" may be bugged server-side:
        // "botAI->CanCastVehicleSpell()" returns SPELL_FAILED_BAD_TARGETS when targeting drakes.
        // Forcing the cast attempt seems to succeed, not sure what's going on here.
        // return CastDrakeSpellAction(target, SPELL_REVIVIFY, 0);
        bool cast = botAI->CastVehicleSpell(SPELL_REVIVIFY, vehicleBase);
        LOG_DEBUG("playerbots", "[EoE debug] {} drake spell {} (forced, no CanCast check) -- {}",
            bot->GetName(), SPELL_REVIVIFY, cast ? "CAST OK" : "Cast FAILED");
        return cast;
    }
}

bool PowerSparkBuffAction::Execute(Event /*event*/)
{
    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit || unit->GetEntry() != NPC_POWER_SPARK || !unit->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
        {
            continue;
        }

        if (bot->GetDistance2d(unit->GetPositionX(), unit->GetPositionY()) > 3.0f)
        {
            return MoveTo(EOE_MAP_ID, unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
        return false;
    }

    return false;
}

bool ArcaneOverloadBubbleAction::Execute(Event /*event*/)
{
    // Stick with whatever bubble was already committed to, instead of re-picking fresh every
    // tick and flip-flopping between bubbles as new ones spawn.
    Unit* unit = !_targetBubble.IsEmpty() ? botAI->GetUnit(_targetBubble) : nullptr;
    if (!unit || !unit->IsInWorld() || unit->GetEntry() != NPC_ARCANE_OVERLOAD)
    {
        unit = nullptr;
    }

    // A bubble's protective radius shrinks continuously over its ~45s life (and up to 3 can be
    // alive at once, given the ~15s respawn cadence -- see boss_malygos.cpp), so being close to
    // its spawn point doesn't mean the buff is still reachable there. If we're at the old 3yd
    // proximity mark but still lack the actual buff, this one's radius has likely shrunk past
    // us -- drop it and pick a fresher one below instead of standing still uselessly.
    if (unit && bot->GetDistance2d(unit->GetPositionX(), unit->GetPositionY()) <= 3.0f &&
        !bot->HasAura(SPELL_ARCANE_OVERLOAD_PROTECTION))
    {
        unit = nullptr;
    }

    if (!unit)
    {
        // Prefer the most recently spawned bubble (highest GUID counter) -- it has the most
        // radius left before it decays to nothing.
        GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
        for (auto& target : targets)
        {
            Unit* candidate = botAI->GetUnit(target);
            if (candidate && candidate->GetEntry() == NPC_ARCANE_OVERLOAD &&
                (!unit || candidate->GetGUID().GetCounter() > unit->GetGUID().GetCounter()))
            {
                unit = candidate;
            }
        }
        if (unit) { _targetBubble = unit->GetGUID(); }
    }
    if (!unit) { return false; }

    if (bot->GetDistance2d(unit->GetPositionX(), unit->GetPositionY()) > 3.0f)
    {
        return MoveTo(EOE_MAP_ID, unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(),
            false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }

    // Already there and the buff-check above didn't reject it -- nothing to move. Return false
    // so lower-priority actions (attacks, heals) still get a turn this tick.
    return false;
}

bool EoEHoverDiskAttackAction::Execute(Event /*event*/)
{
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase) { return false; }

    // Shared with MalygosTargetAction so this rider's flight target and attack target
    // always agree on the same Scion.
    Unit* scion = GetOrPickRiderScion(botAI, bot);
    if (!scion) { return false; }

    // Ranged riders hold spell range and use their own ranged attacks, not melee.
    float engageRange = botAI->IsRangedDps(bot) ? 30.0f : 5.0f;
    if (vehicleBase->GetExactDist(scion) > engageRange)
    {
        // MovePoint to the target's raw position, not MoveChase -- MoveChase kept reissuing
        // every tick without ever closing distance to an airborne target (ground-based
        // navmesh pathing despite SetCanFly()). MovePoint is the pattern proven to work for
        // actual flight elsewhere in this file (EoEFlyDrakeAction).
        MotionMaster* mm = vehicleBase->GetMotionMaster();
        vehicleBase->SetCanFly(true);
        mm->MovePoint(0, scion->GetPositionX(), scion->GetPositionY(), scion->GetPositionZ());
        vehicleBase->SendMovementFlagUpdate();
        return true;
    }

    // Within range -- the disk's seat has no special attack spell like the phase 3 drakes do,
    // so just face the target and let the bot's own normal combat AI take it from here.
    vehicleBase->SetFacingToObject(scion);
    return false;
}

bool EoEHoverDiskAction::Execute(Event /*event*/)
{
    GuidVector npcs = AI_VALUE(GuidVector, "nearest vehicles");
    for (auto& npc : npcs)
    {
        Unit* vehicleBase = botAI->GetUnit(npc);
        if (!vehicleBase || vehicleBase->GetEntry() != NPC_HOVER_DISK) { continue; }

        Vehicle* veh = vehicleBase->GetVehicleKit();
        if (!veh || !veh->GetAvailableSeatCount()) { continue; }

        // 3D distance, not the inherited EnterVehicleAction::EnterVehicle()'s 2D check -- the
        // disk is frequently still elevated (wherever its dead pilot left it, or mid-descent),
        // so a bot standing on the ground beneath one read as "close enough" on a 2D check.
        float dist3d = bot->GetExactDist(vehicleBase);
        if (dist3d > 40.0f) { continue; }

        if (dist3d > INTERACTION_DISTANCE)
        {
            return MoveTo(vehicleBase);
        }

        vehicleBase->HandleSpellClick(bot);
        if (!bot->IsOnVehicle(vehicleBase)) { continue; }

        // Dismount because bots can enter vehicle on mount (same as EnterVehicleAction).
        WorldPacket emptyPacket;
        bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
        return true;
    }

    return false;
}
