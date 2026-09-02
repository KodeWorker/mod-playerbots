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

    // Own trigger, not Oculus's shared "group flying" -- that one also requires the real player
    // (master) to be vehicle-mounted, which would silently disable every bot's Static
    // Field/Surge of Power dodging the moment the master dismounts. See EoEGroupFlyingTrigger.
    //
    // Priority kept above "eoe drake attack" (not just higher than the old ACTION_NORMAL + 1)
    // -- action selection stops at the first one that returns true each tick, so with combat
    // evaluated first, a drake that's successfully attacking basically every tick would starve
    // this of a turn entirely except on the rare tick attack itself fails. EoEFlyDrakeAction
    // only returns true when it actually needs to move (hazard dodge or off-slot formation),
    // so evaluating it first costs nothing on the ticks it doesn't.
    triggers.push_back(new TriggerNode("eoe group flying",
        { NextAction("eoe fly drake", ACTION_NORMAL + 6) }));
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
