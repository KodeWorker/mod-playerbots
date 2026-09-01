#include "EoEStrategy.h"
#include "EoEMultipliers.h"
#include "Strategy.h"

void RaidEoEStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("malygos",
        { NextAction("malygos position", ACTION_MOVE) }));
    // Kept below "arcane overload bubble" -- shelter must win the tick over a fresh target
    // acquisition/switch, or a bot re-targets and starts a melee chase the same tick it needed
    // to relocate to a bubble instead.
    triggers.push_back(new TriggerNode("malygos",
        { NextAction("malygos target", ACTION_RAID) }));

    triggers.push_back(new TriggerNode("group flying",
        { NextAction("eoe fly drake", ACTION_NORMAL + 1) }));
    triggers.push_back(new TriggerNode("drake combat",
        { NextAction("eoe drake attack", ACTION_NORMAL + 5) }));

    triggers.push_back(new TriggerNode("eoe hover disk",
        { NextAction("eoe hover disk", ACTION_MOVE + 2) }));
    triggers.push_back(new TriggerNode("eoe hover disk combat",
        { NextAction("eoe hover disk combat", ACTION_MOVE + 2) }));

    triggers.push_back(new TriggerNode("power spark buff",
        { NextAction("power spark buff", ACTION_MOVE) }));
    // Highest priority in this phase, above even "malygos target" -- taking shelter wins over
    // acquiring/switching a combat target, not just over other movement.
    triggers.push_back(new TriggerNode("arcane overload bubble",
        { NextAction("arcane overload bubble", ACTION_RAID + 1) }));
}

void RaidEoEStrategy::InitMultipliers(std::vector<Multiplier*> &multipliers)
{
    multipliers.push_back(new MalygosMultiplier(botAI));
}
