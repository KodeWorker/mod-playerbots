#ifndef PLAYERBOTS_EOEACTIONCONTEXT_H
#define PLAYERBOTS_EOEACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "EoEActions.h"

class RaidEoEActionContext : public NamedObjectContext<Action>
{
public:
    RaidEoEActionContext()
    {
        creators["malygos position"] = &RaidEoEActionContext::position;
        creators["malygos target"] = &RaidEoEActionContext::target;
        // creators["pull power spark"] = &RaidEoEActionContext::pull_power_spark;
        // creators["kill power spark"] = &RaidEoEActionContext::kill_power_spark;
        creators["eoe fly drake"] = &RaidEoEActionContext::eoe_fly_drake;
        creators["eoe drake attack"] = &RaidEoEActionContext::eoe_drake_attack;
        creators["eoe hover disk"] = &RaidEoEActionContext::eoe_hover_disk;
        creators["power spark buff"] = &RaidEoEActionContext::power_spark_buff;
        creators["arcane overload bubble"] = &RaidEoEActionContext::arcane_overload_bubble;
    }

private:
    static Action* position(PlayerbotAI* ai) { return new MalygosPositionAction(ai); }
    static Action* target(PlayerbotAI* ai) { return new MalygosTargetAction(ai); }
    // static Action* pull_power_spark(PlayerbotAI* ai) { return new PullPowerSparkAction(ai); }
    // static Action* kill_power_spark(PlayerbotAI* ai) { return new KillPowerSparkAction(ai); }
    static Action* eoe_fly_drake(PlayerbotAI* ai) { return new EoEFlyDrakeAction(ai); }
    static Action* eoe_drake_attack(PlayerbotAI* ai) { return new EoEDrakeAttackAction(ai); }
    static Action* eoe_hover_disk(PlayerbotAI* ai) { return new EoEHoverDiskAction(ai); }
    static Action* power_spark_buff(PlayerbotAI* ai) { return new PowerSparkBuffAction(ai); }
    static Action* arcane_overload_bubble(PlayerbotAI* ai) { return new ArcaneOverloadBubbleAction(ai); }
};

#endif
