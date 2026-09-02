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

        // Nexus Lord only -- same reasoning as MalygosTargetAction: the Scion is airborne and
        // unreachable to grounded melee. This action used to grab whichever add came first in
        // the list, so melee could get sent walking toward the Scion's (x,y) forever, stuck
        // chasing a target they could never reach instead of boarding a Hover Disk or
        // sheltering in a bubble.
        Unit* addTarget = nullptr;
        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (unit && unit->GetEntry() == NPC_NEXUS_LORD)
            {
                addTarget = unit;
                break;
            }
        }
        if (!addTarget)
        {
            // Both Nexus Lords are dead -- the only way grounded melee can still contribute is
            // a freed Hover Disk seat (HoverDiskTrigger boards them the instant one opens).
            // There's nothing to walk toward until then, and ceding this tick to a lower-
            // priority combat movement risks chasing a stale/invalid attack state toward the
            // airborne, unreachable Scion -- dragging the bot out of its Arcane Overload
            // bubble. Cancel any leftover movement and hold this slot instead so nothing else
            // claims it; ArcaneOverloadBubbleTrigger keeps re-sheltering independently if the
            // bubble the bot is standing in decays before a disk frees up.
            bot->GetMotionMaster()->Clear(false);
            return true;
        }

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
    // Not gated on botAI->GetMaster()'s own vehicle status -- this used to require the raid's
    // real-player master to be mounted on a Skytalon too, which meant one player dismounting
    // (dying, disconnecting, manually getting off) silently disabled Static Field dodging,
    // Surge of Power fleeing, and formation positioning for *every* bot in the raid. None of
    // that logic actually needs the master anymore now that formation is boss-anchored.
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase) { return false; }

    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss) { return false; }

    MotionMaster* mm = vehicleBase->GetMotionMaster();

    // Marked for the incoming Surge of Power blast (25man only, see the constant's comment) --
    // break formation and fly away from the boss (and by extension the raid, which clusters
    // near it) so the AoE doesn't catch everyone else too.
    if (vehicleBase->HasAura(SPELL_SURGE_OF_POWER_WARN_SELECTOR_25))
    {
        mm->Clear(false);
        float angle = boss->GetAngle(vehicleBase);  // bearing from the boss toward the bot
        float fleeDist = 25.0f;
        float x = vehicleBase->GetPositionX() + cos(angle) * fleeDist;
        float y = vehicleBase->GetPositionY() + std::sin(angle) * fleeDist;
        vehicleBase->SetCanFly(true);
        mm->MovePoint(0, x, y, vehicleBase->GetPositionZ());
        vehicleBase->SendMovementFlagUpdate();
        // Invalidate the last commanded cluster point -- once the mark clears (or on the very
        // next tick otherwise), the formation logic below unconditionally re-aims and rejoins
        // the pack immediately, instead of waiting on the settled-but-off-slot distance check
        // to notice the drift from this detour.
        _hasFormationTarget = false;
        return true;
    }

    // Static Field is a stationary hazard dealing periodic damage for ~20s
    // (EVENT_SPELL_STATIC_FIELD in boss_malygos.cpp), landing on a random player. Per the
    // strategy guide, the whole group relocates together when it lands, not just whoever it
    // happened to land on -- staying stacked is what makes AoE healing effective, so splitting
    // off individually defeats the point. Detected once here and used to steer the shared
    // formation anchor below; still checked for immediate personal danger first, since group
    // relocation (below) takes a few ticks to complete via MovePoint.
    GuidVector nearbyNpcs = AI_VALUE(GuidVector, "nearest npcs");
    Unit* staticField = nullptr;
    for (auto& npc : nearbyNpcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == NPC_STATIC_FIELD) { staticField = unit; break; }
    }

    if (staticField)
    {
        float dangerRadius = 15.0f;  // estimate -- radius isn't queryable from this DB
        if (vehicleBase->GetExactDist2d(staticField) <= dangerRadius)
        {
            mm->Clear(false);
            float angle = staticField->GetAngle(vehicleBase);  // bearing from the hazard toward the bot
            float fleeDist = dangerRadius + 10.0f;
            float x = staticField->GetPositionX() + cos(angle) * fleeDist;
            float y = staticField->GetPositionY() + std::sin(angle) * fleeDist;
            vehicleBase->SetCanFly(true);
            mm->MovePoint(0, x, y, vehicleBase->GetPositionZ());
            vehicleBase->SendMovementFlagUpdate();
            // Same reasoning as the Surge of Power branch above -- rejoin the (now-shifted)
            // cluster immediately once out of immediate danger, instead of waiting on the
            // distance check.
            _hasFormationTarget = false;
            return true;
        }
    }

    // Per the strategy guide: "all drakes group up on one spot, roughly level with Malygos...
    // at least 30 yards away, and close enough to use Flame Spike" -- a tight cluster anchored
    // on the boss, not a wide ring around a peer bot. Surge of Power (single-target beam) isn't
    // a clump punisher, so there's no reason to spread out the way phase 2's bubbles required.
    // The anchor's bearing off the boss is pushed away from a live Static Field so every drake
    // (running this same deterministic formula off the same boss/hazard) re-aims to the same
    // side together, instead of only the one bot that happened to be standing in it.
    int32 numPlayers = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? 25 : 10;
    float engageRange = 40.0f;     // comfortably past the 30yd minimum, within Flame Spike range
    float clusterRadius = 6.0f;    // tight grouping, just enough to avoid literal stacking
    float baseAngle = staticField ? (boss->GetAngle(staticField) + static_cast<float>(M_PI)) : 0.0f;
    float clusterAngle = botAI->GetGroupSlotIndex(bot) * (2.0f * static_cast<float>(M_PI)) / numPlayers;
    float idealX = boss->GetPositionX() + cos(baseAngle) * engageRange + cos(clusterAngle) * clusterRadius;
    float idealY = boss->GetPositionY() + std::sin(baseAngle) * engageRange + std::sin(clusterAngle) * clusterRadius;

    // Re-aim only when meaningfully off-slot, not every tick -- MovePoint toward a target
    // recalculated fresh every tick (the boss orbits continuously) never let the drake actually
    // arrive and go idle. isMoving() then read true almost permanently, which silently blocked
    // every cast-time drake spell via CanCastVehicleSpell()'s moving check -- Flame Shield
    // (a real 30s-cooldown defensive, not a maintenance buff -- see EoEDrakeAttackAction)
    // included; the whole encounter logged zero successful casts of it before this fix.
    bool driftedFromLastAim = !_hasFormationTarget ||
        std::sqrt(std::pow(idealX - _formationTargetX, 2.0f) + std::pow(idealY - _formationTargetY, 2.0f)) > 15.0f;
    bool settledButOffSlot = !vehicleBase->isMoving() &&
        vehicleBase->GetExactDist2d(idealX, idealY) > clusterRadius + 5.0f;

    if (driftedFromLastAim || settledButOffSlot)
    {
        _hasFormationTarget = true;
        _formationTargetX = idealX;
        _formationTargetY = idealY;
        vehicleBase->SetCanFly(true);
        mm->MovePoint(0, idealX, idealY, boss->GetPositionZ());
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

    // Flame Shield is a reactive defensive with a real 30s cooldown (the server enforces it via
    // HasSpellCooldown inside CanCastVehicleSpell -- the 0 below is just our own no-op extra
    // tracking), not a maintenance buff -- per the guide, the player marked by Surge of Power
    // "MUST use Flame Shield or risk dying". SPELL_SURGE_OF_POWER_WARN_SELECTOR_25 (25man only)
    // is checked here implicitly by trying every tick: whoever gets marked casts it the moment
    // the aura lands, since this is the first thing tried each tick. 10man has no detectable
    // marker, so recasting on cooldown whenever missing is the best available baseline there.
    //
    // Flame Shield is itself a combo-point finisher (Spell::CheckCast's m_needComboPoints branch
    // needs at least 1), and its shield duration scales with how many are spent -- 2s/3s/4s/5s/6s
    // at 1/2/3/4/5 CP. The real Surge of Power warn-to-impact gap is a fixed 3s
    // (EVENT_SPELL_PH3_SURGE_OF_POWER in boss_malygos.cpp), so 1 CP (2s) doesn't cover it -- hold
    // out for 2 CP (3s) before spending. Below that, build instead of blindly attempting the cast
    // every tick: attempting it at 0 CP wasn't just a wasted attempt, it starved combo generation
    // entirely, since this ran before DrakeDpsAction/DrakeHealAction -- the only things that ever
    // build combo points -- got a turn. The whole encounter logged zero successful Flame Shield
    // casts before this fix.
    uint8 const minComboPointsForFlameShield = 2;
    if (!vehicleBase->HasAura(SPELL_FLAME_SHIELD))
    {
        uint8 comboPoints = vehicleBase->GetComboPoints();
        if (comboPoints < minComboPointsForFlameShield)
        {
            // DPS role builds combo points here via Flame Spike; heal role builds them via
            // DrakeHealAction's Revivify below instead, so just fall through for healers.
            if (boss && !botAI->IsHeal(bot))
            {
                return CastDrakeSpellAction(boss, SPELL_FLAME_SPIKE, 0);
            }
        }
        else if (CastDrakeSpellAction(vehicleBase, SPELL_FLAME_SHIELD, 0))
        {
            return true;
        }
    }

    // Marked bot also pops Blazing Speed to help EoEFlyDrakeAction's flee maneuver (the same
    // SPELL_SURGE_OF_POWER_WARN_SELECTOR_25 aura) actually outrun the beam, not just tank it.
    if (vehicleBase->HasAura(SPELL_SURGE_OF_POWER_WARN_SELECTOR_25) &&
        !vehicleBase->HasAura(SPELL_BLAZING_SPEED) &&
        CastDrakeSpellAction(vehicleBase, SPELL_BLAZING_SPEED, 0))
    {
        return true;
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
    // us -- drop it and pick a fresher one below instead of standing still uselessly. Radius
    // only ever shrinks, never regrows, so blacklist it for the rest of its life -- otherwise,
    // when it's the only bubble around, "pick the freshest" below just walks straight back onto
    // this same decayed one next tick: still inside the stale 3yd mark, no MoveTo ever fires,
    // and the bot sits there FAILing forever instead of holding position and waiting for the
    // next one to spawn (~15s cadence).
    if (unit && bot->GetDistance2d(unit->GetPositionX(), unit->GetPositionY()) <= 3.0f &&
        !bot->HasAura(SPELL_ARCANE_OVERLOAD_PROTECTION))
    {
        _decayedBubbles.push_back(unit->GetGUID());
        unit = nullptr;
    }

    if (!unit)
    {
        // Prefer the most recently spawned bubble (highest GUID counter) -- it has the most
        // radius left before it decays to nothing.
        GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
        for (auto& target : targets)
        {
            if (std::find(_decayedBubbles.begin(), _decayedBubbles.end(), target) != _decayedBubbles.end())
            {
                continue;
            }
            Unit* candidate = botAI->GetUnit(target);
            if (candidate && candidate->GetEntry() == NPC_ARCANE_OVERLOAD &&
                (!unit || candidate->GetGUID().GetCounter() > unit->GetGUID().GetCounter()))
            {
                unit = candidate;
            }
        }
        if (unit) { _targetBubble = unit->GetGUID(); }
        else { _targetBubble.Clear(); }
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
