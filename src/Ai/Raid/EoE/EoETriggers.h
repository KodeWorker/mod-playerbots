#ifndef PLAYERBOTS_EOETRIGGERS_H
#define PLAYERBOTS_EOETRIGGERS_H

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Trigger.h"

enum EyeOfEternityIDs
{
    NPC_MALYGOS                         = 28859,
    NPC_POWER_SPARK                     = 30084,
    NPC_NEXUS_LORD                      = 30245,
    NPC_SCION_OF_ETERNITY               = 30249,
    NPC_WYRMREST_SKYTALON               = 30161,
    NPC_HOVER_DISK                      = 30248,
    NPC_ARCANE_OVERLOAD                 = 30282,
    NPC_STATIC_FIELD                    = 30592,

    SPELL_ARCANE_OVERLOAD                = 56430,
    SPELL_ARCANE_OVERLOAD_DMG            = 56431,
    SPELL_ARCANE_OVERLOAD_PROTECTION     = 56438,

    SPELL_POWER_SPARK_VISUAL            = 55845,
    SPELL_POWER_SPARK_GROUND_BUFF       = 55852,
    SPELL_POWER_SPARK_MALYGOS_BUFF      = 56152,

    SPELL_TELEPORT_VISUAL               = 52096,

    SPELL_SCION_ARCANE_BARRAGE          = 56397,
    SPELL_ARCANE_SHOCK_N                = 57058,
    SPELL_ARCANE_SHOCK_H                = 60073,
    SPELL_HASTE                         = 57060,

    SPELL_ALEXSTRASZA_GIFT              = 61028,

    // Phase 3: 25man warns the marked drake rider with this aura ~3s before the AoE lands
    // (SPELL_PH3_SURGE_OF_POWER_25) -- 10man has no equivalent detectable marker, only an
    // internal boss-script GUID and a chat emote to that player, neither queryable here.
    SPELL_SURGE_OF_POWER_WARN_SELECTOR_25 = 60939,

    // Drake Abilities:
    // DPS
    SPELL_FLAME_SPIKE                   = 56091,
    SPELL_ENGULF_IN_FLAMES              = 56092,
    // Healing
    SPELL_REVIVIFY                      = 57090,
    SPELL_LIFE_BURST                    = 57143,
    // Utility
    SPELL_FLAME_SHIELD                  = 57108,
    SPELL_BLAZING_SPEED                 = 57092,
};

const uint32 EOE_MAP_ID = 616;

class MalygosTrigger : public Trigger
{
public:
    MalygosTrigger(PlayerbotAI* botAI) : Trigger(botAI, "malygos") {}
    bool IsActive() override;
    uint8 static getPhase(Player* bot, Unit* boss);
};

class PowerSparkTrigger : public Trigger
{
public:
    PowerSparkTrigger(PlayerbotAI* botAI) : Trigger(botAI, "power spark") {}
    bool IsActive() override;
};

// Phase 2: a Hover Disk's seat frees up once its Nexus Lord/Scion pilot dies, letting melee
// reach the otherwise-unreachable airborne Scion (see HoverDiskCombatTrigger for the actual
// engage). Ranged/tank can ride too; healers stay off it and keep healing.
class HoverDiskTrigger : public Trigger
{
public:
    HoverDiskTrigger(PlayerbotAI* botAI) : Trigger(botAI, "eoe hover disk") {}
    bool IsActive() override;
};

// Phase 1: a killed Power Spark leaves a ground buff at its death position (+50% damage,
// SPELL_POWER_SPARK_GROUND_BUFF -- self-cast by the spark, see npc_power_spark::DamageTaken
// in boss_malygos.cpp). Ranged DPS without the buff walk to it; melee stays on Malygos.
class PowerSparkGroundBuffTrigger : public Trigger
{
public:
    PowerSparkGroundBuffTrigger(PlayerbotAI* botAI) : Trigger(botAI, "power spark buff") {}
    bool IsActive() override;
};

// Phase 2: standing inside an Arcane Overload's blast radius grants a protective shield
// (SPELL_ARCANE_OVERLOAD_PROTECTION) against Malygos's raid-wide AoE. Per the strategy guide,
// all grounded players (melee included) continuously shelter between bubbles, moving to a new
// one as each expires; attacking still happens alongside this. Only vehicle riders are
// excluded (attack only).
class ArcaneOverloadBubbleTrigger : public Trigger
{
public:
    ArcaneOverloadBubbleTrigger(PlayerbotAI* botAI) : Trigger(botAI, "arcane overload bubble") {}
    bool IsActive() override;
};

// Phase 2: once seated on a Hover Disk (see HoverDiskTrigger), fly it to the Scion of
// Eternity and engage -- the actual point of boarding, per the strategy guide.
class HoverDiskCombatTrigger : public Trigger
{
public:
    HoverDiskCombatTrigger(PlayerbotAI* botAI) : Trigger(botAI, "eoe hover disk combat") {}
    bool IsActive() override;
};

#endif
