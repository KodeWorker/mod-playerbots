/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SpellIdValue.h"

#include "ChatHelper.h"
#include "Playerbots.h"
#include "Vehicle.h"

SpellIdValue::SpellIdValue(PlayerbotAI* botAI) : CalculatedValue<uint32>(botAI, "spell id", 20 * 1000) {}

VehicleSpellIdValue::VehicleSpellIdValue(PlayerbotAI* botAI) : CalculatedValue<uint32>(botAI, "vehicle spell id") {}

uint32 SpellIdValue::Calculate()
{
    std::string namepart = qualifier;
    ItemIds itemIds = ChatHelper::parseItems(namepart);

    PlayerbotChatHandler handler(bot);
    uint32 extractedSpellId = handler.extractSpellId(namepart);
    if (extractedSpellId)
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(extractedSpellId))
            namepart = ChatHelper::GetLocalizedSpellName(spellInfo);

    std::wstring wnamepart;
    if (!Utf8toWStr(namepart, wnamepart))
        return 0;

    wstrToLower(wnamepart);
    char firstSymbol = tolower(namepart[0]);
    size_t spellLength = wnamepart.length();

    // Callers pass either a hardcoded English literal baked into strategy code (the vast
    // majority, e.g. AI_VALUE2(uint32, "spell id", "judgement")) or - after resolving a
    // spell link/ID typed by the master - the spell's name in the display locale. Match
    // against both the enUS and the localized name so name resolution works regardless
    // of which one namepart actually is.
    auto matchesSpellName = [&](SpellInfo const* spellInfo)
    {
        char const* enUSName = spellInfo->SpellName[LOCALE_enUS];
        if (enUSName && *enUSName && tolower(enUSName[0]) == firstSymbol && strlen(enUSName) == spellLength &&
            Utf8FitTo(enUSName, wnamepart))
            return true;

        char const* localizedName = ChatHelper::GetLocalizedSpellName(spellInfo);
        if (localizedName && *localizedName && tolower(localizedName[0]) == firstSymbol &&
            strlen(localizedName) == spellLength && Utf8FitTo(localizedName, wnamepart))
            return true;

        return false;
    };

    std::set<uint32> spellIds;
    for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;

        if (itr->second->State == PLAYERSPELL_REMOVED || !itr->second->Active)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || spellInfo->IsPassive())
            continue;

        if (spellInfo->Effects[0].Effect == SPELL_EFFECT_LEARN_SPELL)
            continue;

        bool useByItem = false;
        for (uint8 i = 0; i < 3; ++i)
        {
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_CREATE_ITEM &&
                itemIds.find(spellInfo->Effects[i].ItemType) != itemIds.end())
            {
                useByItem = true;
                break;
            }
        }

        if (!useByItem && !matchesSpellName(spellInfo))
            continue;

        spellIds.insert(spellId);
    }

    Pet* pet = bot->GetPet();
    if (spellIds.empty() && pet)
    {
        for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
        {
            if (itr->second.state == PETSPELL_REMOVED)
                continue;

            uint32 spellId = itr->first;
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!spellInfo)
                continue;

            if (spellInfo->Effects[0].Effect == SPELL_EFFECT_LEARN_SPELL)
                continue;

            if (!matchesSpellName(spellInfo))
                continue;

            spellIds.insert(spellId);
        }
    }

    if (spellIds.empty())
        return 0;

    int32 saveMana = (int32)round(AI_VALUE(double, "mana save level"));
    uint32 rank = 1;
    uint32 highestRank = 0;
    uint32 highestSpellId = 0;
    uint32 lowestRank = 0;
    uint32 lowestSpellId = 0;
    if (saveMana <= 1)
    {
        for (auto it = spellIds.rbegin(); it != spellIds.rend(); ++it)
        {
            auto spellId = *it;
            const SpellInfo* pSpellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!pSpellInfo)
                continue;

            std::string spellName = pSpellInfo->Rank[0];

            // For atoi, the input string has to start with a digit, so lets search for the first digit
            size_t i = 0;
            for (; i < spellName.length(); i++)
            {
                if (isdigit(spellName[i]))
                    break;
            }

            // remove the first chars, which aren't digits
            spellName = spellName.substr(i, spellName.length() - i);

            // convert the remaining text to an integer
            int id = atoi(spellName.c_str());

            if (!id)
            {
                highestSpellId = spellId;
                continue;
            }

            if (!highestRank || (uint32)id > highestRank)
            {
                highestRank = id;
                highestSpellId = spellId;
            }

            if (!lowestRank || (lowestRank && (uint32)id < lowestRank))
            {
                lowestRank = id;
                lowestSpellId = spellId;
            }
        }
    }
    else
    {
        for (auto it = spellIds.rbegin(); it != spellIds.rend(); ++it)
        {
            auto spellId = *it;
            if (!highestSpellId)
                highestSpellId = spellId;
            if (saveMana == (int32)rank)
                return spellId;
            lowestSpellId = spellId;
            rank++;
        }
    }

    return saveMana > 1 ? lowestSpellId : highestSpellId;
}

uint32 VehicleSpellIdValue::Calculate()
{
    Vehicle* vehicle = bot->GetVehicle();
    if (!vehicle)
        return 0;

    // do not allow if no spells
    VehicleSeatEntry const* seat = vehicle->GetSeatForPassenger(bot);
    if (!seat || !(seat->m_flags & VEHICLE_SEAT_FLAG_CAN_CAST))
        return 0;

    Unit* vehicleBase = vehicle->GetBase();
    if (!vehicleBase->IsAlive())
        return 0;

    std::string namepart = qualifier;

    PlayerbotChatHandler handler(bot);
    uint32 extractedSpellId = handler.extractSpellId(namepart);
    if (extractedSpellId)
        if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(extractedSpellId))
            namepart = ChatHelper::GetLocalizedSpellName(spellInfo);

    std::wstring wnamepart;
    if (!Utf8toWStr(namepart, wnamepart))
        return 0;

    wstrToLower(wnamepart);
    char firstSymbol = tolower(namepart[0]);
    size_t spellLength = wnamepart.length();

    // See the matching comment in SpellIdValue::Calculate: match against both the enUS
    // and the localized name, since namepart may be either depending on the caller.
    auto matchesSpellName = [&](SpellInfo const* spellInfo)
    {
        char const* enUSName = spellInfo->SpellName[LOCALE_enUS];
        if (enUSName && *enUSName && tolower(enUSName[0]) == firstSymbol && strlen(enUSName) == spellLength &&
            Utf8FitTo(enUSName, wnamepart))
            return true;

        char const* localizedName = ChatHelper::GetLocalizedSpellName(spellInfo);
        if (localizedName && *localizedName && tolower(localizedName[0]) == firstSymbol &&
            strlen(localizedName) == spellLength && Utf8FitTo(localizedName, wnamepart))
            return true;

        return false;
    };

    Creature* creature = vehicleBase->ToCreature();
    for (uint32 x = 0; x < MAX_CREATURE_SPELLS; ++x)
    {
        uint32 spellId = creature->m_spells[x];
        if (spellId == 2)
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || spellInfo->IsPassive())
            continue;

        if (!matchesSpellName(spellInfo))
            continue;

        return spellId;
    }

    return 0;
}
