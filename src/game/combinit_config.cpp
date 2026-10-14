#include "pch.h"
#include "game/combinit.h"

namespace COMBINIT {
    int16_t max_ships_per_side = ORIGINAL_MAX_COMBAT_SHIPS_PER_SIDE;
    int16_t max_flyers_per_side = ORIGINAL_MAX_COMBAT_FLYERS;
    int16_t max_flyers_total = ORIGINAL_MAX_COMBAT_FLYERS;

    uint8_t starbase_special
        [STARBASE_SPECIAL_SLOT_COUNT]
        [STARBASE_SPECIAL_ALTERNATIVE_COUNT] = {
            {SPECIAL_REINFORCED_HULL, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_SHIELD_CAPACITOR, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_HARD_SHIELDS, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_MULTI_PHASED_SHIELDS, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_WIDE_AREA_JAMMER, SPECIAL_MULTI_WAVE_ECM_JAMMER, SPECIAL_ECM_JAMMER},
            {SPECIAL_WARP_DISSIPATOR, SPECIAL_WARP_DISSIPATOR, SPECIAL_WARP_DISSIPATOR},
            {SPECIAL_SECURITY_STATIONS, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_BATTLE_SCANNER, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_HIGH_ENERGY_FOCUS, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_STRUCTURAL_ANALYZER, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_ACHILLES_TARGETING_UNIT, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_RANGEMASTER_UNIT, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_TRANSPORTERS, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
            {SPECIAL_TRANSPORTERS, SPECIAL_NO_SPECIAL, SPECIAL_NO_SPECIAL},
    };

    s_defenses_allowed_mods defenses_allowed_mods = {
        WEAPON_MOD_FLAG_HEAVY_MOUNT |
            WEAPON_MOD_FLAG_POINT_DEFENSE |
            WEAPON_MOD_FLAG_ARMOR_PIERCING |
            WEAPON_MOD_FLAG_CONTINUOUS |
            WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_SHIELD_PIERCING |
            WEAPON_MOD_FLAG_AUTO_FIRE |
            WEAPON_MOD_FLAG_ENVELOPING,
        WEAPON_MOD_FLAG_HEAVY_MOUNT |
            WEAPON_MOD_FLAG_POINT_DEFENSE |
            WEAPON_MOD_FLAG_ARMOR_PIERCING |
            WEAPON_MOD_FLAG_CONTINUOUS |
            WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_SHIELD_PIERCING |
            WEAPON_MOD_FLAG_AUTO_FIRE |
            WEAPON_MOD_FLAG_ENVELOPING,
        WEAPON_MOD_FLAG_HEAVY_MOUNT |
            WEAPON_MOD_FLAG_POINT_DEFENSE |
            WEAPON_MOD_FLAG_CONTINUOUS |
            WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_AUTO_FIRE |
            WEAPON_MOD_FLAG_ENVELOPING,
        WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_ENVELOPING |
            WEAPON_MOD_FLAG_ECCM |
            WEAPON_MOD_FLAG_HEAVILY_ARMORED |
            WEAPON_MOD_FLAG_FAST |
            WEAPON_MOD_FLAG_OVERLOADED,
    };
    s_satellite_space_allocation satellite_space_allocation = {
        26,
        13,
        12,
        49,
    };
    s_reinforced_hull_multiplier reinforced_hull_multiplier = {
        3,
        3,
    };
    uint8_t antaran_fortress_armor_multiplier = 3;
    s_ground_defense ground_defense = {
        300,
        125,
        2,
        5,
        3,
        2,
        1,
        10,
        338,
        112,
        1,
    };
    uint8_t satellite_ammo_basis = 20;
    uint8_t missile_base_ammo_basis = 5;
    s_planet_extra_specials planet = {};
    uint16_t antaran_star_fortress_space = 1500;
    s_repair_rate repair_rate = {10, 20, 20};
    s_shield_recharge_rate shield_recharge_rate = {3, 10};
    uint8_t amoeba_regeneration = 0;
    uint8_t crystal_keep = 0;
    uint8_t planet_damper_enabled = 0;
    uint8_t satellite_capture_enabled = 0;
    uint8_t satellite_warlord_bonus = 0;
    uint8_t satellite_size_class_plus_one = 0;
    uint8_t scroll_interval = 1;
    e_plasma_web_dissipation plasma_web_dissipation =
        PLASMA_WEB_DISSIPATION_SUBTRACT_FIVE;
} // namespace COMBINIT
