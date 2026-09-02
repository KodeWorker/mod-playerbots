#ifndef PLAYERBOTS_EOEACTIONS_H
#define PLAYERBOTS_EOEACTIONS_H

#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "VehicleActions.h"

const std::pair<float, float> MALYGOS_MAINTANK_POSITION = {757.0f, 1337.0f};
const std::pair<float, float> MALYGOS_STACK_POSITION = {755.0f, 1301.0f};

// Matches CenterPos in boss_malygos.cpp. Phase 2's Surge of Power ("Deep Breath") only ever
// brings the boss this close to room center once per ~65s cycle, when it flies in to cast it
// (EVENT_MOVE_TO_SURGE_OF_POWER/EVENT_SPELL_SURGE_OF_POWER) -- circling between casts orbits
// far out at Phase2NorthPos's radius (~83yd). Used as a cheap, no-core-change proxy for "the
// raid-wide AoE is imminent, go shelter now" -- see ArcaneOverloadBubbleTrigger.
const std::pair<float, float> MALYGOS_CENTER_POSITION = {754.395f, 1301.27f};
const float EOE_SURGE_IMMINENT_RADIUS = 30.0f;

class MalygosPositionAction : public MovementAction
{
public:
    MalygosPositionAction(PlayerbotAI* botAI, std::string const name = "malygos position") : MovementAction(botAI, name)
    {
    }

    bool Execute(Event event) override;
};

class MalygosTargetAction : public AttackAction
{
public:
    MalygosTargetAction(PlayerbotAI* botAI, std::string const name = "malygos target") : AttackAction(botAI, name) {}

    bool Execute(Event event) override;
};

//class PullPowerSparkAction : public CastSpellAction
//{
//public:
//    PullPowerSparkAction(PlayerbotAI* botAI, std::string const name = "pull power spark") : CastSpellAction(botAI, name)
//    {
//    }

//    bool Execute(Event event) override;
//    bool isUseful() override;
//    bool isPossible() override;
//};

class KillPowerSparkAction : public AttackAction
{
public:
    KillPowerSparkAction(PlayerbotAI* botAI, std::string const name = "kill power spark") : AttackAction(botAI, name) {}

    bool Execute(Event event) override;
};

class EoEFlyDrakeAction : public MovementAction
{
public:
    EoEFlyDrakeAction(PlayerbotAI* ai) : MovementAction(ai, "eoe fly drake") {}

    bool Execute(Event event) override;
    bool isPossible() override;

private:
    // Last formation point actually commanded via MovePoint, and whether one has been set yet.
    // Re-issuing a fresh MovePoint every tick toward a constantly-recalculated slot never let
    // the drake settle (isMoving() stayed true almost permanently), silently blocking every
    // cast-time drake spell -- Flame Shield included. See the .cpp for the hysteresis this
    // enables.
    bool _hasFormationTarget = false;
    float _formationTargetX = 0.0f;
    float _formationTargetY = 0.0f;
};

class EoEDrakeAttackAction : public Action
{
public:
    EoEDrakeAttackAction(PlayerbotAI* botAI) : Action(botAI, "eoe drake attack") {}

    bool Execute(Event event) override;
    bool isPossible() override;

protected:
    Unit* vehicleBase;
    bool CastDrakeSpellAction(Unit* target, uint32 spellId, uint32 cooldown);
    bool DrakeDpsAction(Unit* target);
    bool DrakeHealAction();
};

// Phase 2: board a Hover Disk to reach the airborne Scion of Eternity (see HoverDiskTrigger).
class EoEHoverDiskAction : public EnterVehicleAction
{
public:
    EoEHoverDiskAction(PlayerbotAI* botAI) : EnterVehicleAction(botAI, "eoe hover disk") {}

    bool Execute(Event event) override;
};

// Phase 1: walk to a killed Power Spark's ground buff (see PowerSparkGroundBuffTrigger).
class PowerSparkBuffAction : public MovementAction
{
public:
    PowerSparkBuffAction(PlayerbotAI* botAI) : MovementAction(botAI, "power spark buff") {}

    bool Execute(Event event) override;
};

// Phase 2: fly a boarded Hover Disk to the Scion of Eternity and engage (see HoverDiskCombatTrigger).
class EoEHoverDiskAttackAction : public MovementAction
{
public:
    EoEHoverDiskAttackAction(PlayerbotAI* botAI) : MovementAction(botAI, "eoe hover disk combat") {}

    bool Execute(Event event) override;
};

// Phase 2: walk into an Arcane Overload's protective blast radius (see ArcaneOverloadBubbleTrigger).
class ArcaneOverloadBubbleAction : public MovementAction
{
public:
    ArcaneOverloadBubbleAction(PlayerbotAI* botAI) : MovementAction(botAI, "arcane overload bubble") {}

    bool Execute(Event event) override;

private:
    // Sticks with whatever bubble was last committed to, instead of re-picking "nearest"
    // every tick -- see the .cpp for why.
    ObjectGuid _targetBubble;
};

#endif
