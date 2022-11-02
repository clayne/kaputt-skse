#include "trigger.h"

#include "re.h"
#include <effolkronium/random.hpp>

namespace kaputt
{
bool PostHitTrigger::process(RE::Actor* victim, RE::HitData& hit_data)
{
    if (!enabled)
        return true;

    auto kap = Kaputt::getSingleton();
    if (!kap->isReady())
        return true;

    auto attacker = hit_data.aggressor.get().get();
    if (!attacker || !victim)
        return true;

    // logger::debug("{} hitting {}", attacker->GetName(), victim->GetName());

    // 0-no 1-exec 2-killmove
    // execution check
    uint8_t do_trigger = enable_bleedout_execution && victim->AsActorState()->IsBleedingOut();
    bool    getting_up = (victim->AsActorState()->GetKnockState() == RE::KNOCK_STATE_ENUM::kGetUp) ||
        (victim->AsActorState()->GetKnockState() == RE::KNOCK_STATE_ENUM::kQueued);
    if (!do_trigger)
        do_trigger = enable_getup_execution && getting_up;
    // damage check
    if (!do_trigger)
    {
        float dmg_mult = getDamageMult(victim->IsPlayerRef());
        if (victim->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) <= hit_data.totalDamage * dmg_mult)
            do_trigger = 2;
    }

    if (!do_trigger)
        return true;

    if (!kap->precondition(attacker, victim))
        return true;

    if (!lottery(attacker, victim, do_trigger == 1))
        return true;

    if (!kap->submit(attacker, victim))
        return true;
    return !getting_up;
}

bool PostHitTrigger::lottery(RE::Actor* attacker, RE::Actor* victim, bool is_exec)
{
    size_t idx  = -1 + 2 * !attacker->IsPlayerRef() + !victim->IsPlayerRef();
    float  prob = (is_exec ? prob_exec : prob_km)[idx];
    return effolkronium::random_static::get(0.f, 100.f) < prob;
}

void SneakTrigger::process(uint32_t scancode)
{
    if (!enabled)
        return;

    auto kap = Kaputt::getSingleton();
    if (!kap->isReady())
        return;

    if (scancode != key_scancode)
        return;

    auto target_ref = RE::CrosshairPickData::GetSingleton()->targetActor;
    if (!target_ref || !target_ref.get())
        return;

    auto target = target_ref.get()->As<RE::Actor>();
    if (!target)
        return;

    auto player = RE::PlayerCharacter::GetSingleton();
    if (!player)
        return;

    // stealth check
    if (getDetected(player, target))
        return;
    if (need_crouch && !player->IsSneaking())
        return;

    if (!kap->precondition(player, target))
        return;

    kap->submit(player, target, need_crouch ? SubmitInfoStruct{} : SubmitInfoStruct{.required_tags = {"sneak"}});
    return;
}
} // namespace kaputt