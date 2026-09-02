#ifndef PLAYERBOTS_EOETRIGGERCONTEXT_H
#define PLAYERBOTS_EOETRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "EoETriggers.h"

class RaidEoETriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidEoETriggerContext()
    {
        creators["malygos"] = &RaidEoETriggerContext::malygos;
        creators["power spark"] = &RaidEoETriggerContext::power_spark;
        creators["eoe hover disk"] = &RaidEoETriggerContext::hover_disk;
        creators["power spark buff"] = &RaidEoETriggerContext::power_spark_buff;
        creators["arcane overload bubble"] = &RaidEoETriggerContext::arcane_overload_bubble;
        creators["eoe hover disk combat"] = &RaidEoETriggerContext::hover_disk_combat;
        creators["eoe group flying"] = &RaidEoETriggerContext::eoe_group_flying;
    }

private:
    static Trigger* power_spark(PlayerbotAI* ai) { return new PowerSparkTrigger(ai); }
    static Trigger* malygos(PlayerbotAI* ai) { return new MalygosTrigger(ai); }
    static Trigger* hover_disk(PlayerbotAI* ai) { return new HoverDiskTrigger(ai); }
    static Trigger* power_spark_buff(PlayerbotAI* ai) { return new PowerSparkGroundBuffTrigger(ai); }
    static Trigger* arcane_overload_bubble(PlayerbotAI* ai) { return new ArcaneOverloadBubbleTrigger(ai); }
    static Trigger* hover_disk_combat(PlayerbotAI* ai) { return new HoverDiskCombatTrigger(ai); }
    static Trigger* eoe_group_flying(PlayerbotAI* ai) { return new EoEGroupFlyingTrigger(ai); }
};

#endif
