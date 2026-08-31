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

private:
    // Sticks with whatever Scion was last committed to, instead of re-picking "first match"
    // fresh every tick -- see the .cpp for why.
    ObjectGuid _targetScion;
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
