#include "pch.h"

#include "game/aitk_common.h"

#include "game/aitech.h"

namespace AITK_TECH {

  static uint __cdecl Consistent_Random_(uint n) {
    // TODO: fast exp if n big
    uint seed = MOX::settings.start_of_game_seed;
    for (uint i = 0; i < n; i++) {
      seed = seed * 0x41c64e6d + 0x3039;
    }
    return seed;
  }

// Research
  
  static void __cdecl Set_Research_One_Player_(struct AITK::aidata * aidata, int player_id, uint8_t * preferred_tech) {
    /* TODO :
       - add pre-warp techs
       - be more subtle ?
       - specific path (+ build order) for tele / +50 ship attack ?
    */

    struct s_player * player = MOX::player+player_id;

    *preferred_tech = -1;

    bool needs_new_tech = false;
    if (player->tech_fields[(uint8_t)player->current_research_field] == 3 ||
        player->got_new_app != 0 ||
        player->research_breakthrough != 0) {
      needs_new_tech = true;
    }
    player->got_new_app = 0;
    player->research_breakthrough = 0;
    //if (!needs_new_tech) { return; }

    int no_trading_contact = 1;
    if (!player->traits[TRAIT_REPULSIVE] &&
        !player->traits[TRAIT_CREATIVE]) {
      for (int i = 0; i < MOX::NUM_PLAYERS; i++) {
        if (i != player_id && player->contact[i]
            && !MOX::player[i].traits[TRAIT_REPULSIVE]
            && !MOX::player[i].traits[TRAIT_CREATIVE]) {
          no_trading_contact = 0;
        }
      }
    }

    int advanced_construction_choice = TECH_APP_AUTOMATED_FACTORIES;
    if (!player->traits[TRAIT_REPULSIVE] && !player->traits[TRAIT_CREATIVE]) {
      int proba_missiles = 10;
      if (player->traits[TRAIT_ENVIRONMENT_IMMUNE]) { proba_missiles += 16; }
      if (player->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_UNIFICATION / 2) { proba_missiles += 8; }
      int r = Consistent_Random_(player_id+1) % 100;
      if (r < proba_missiles) {
        advanced_construction_choice = TECH_APP_PLANETARY_MISSILE_BASE;
      } else if (r < 2*proba_missiles) {
        advanced_construction_choice = TECH_APP_HEAVY_ARMOR;
      }
    }

    int isolated = 0;
    /* TODO: restore this
       {
       byte cc_size = connected_component_size[pid][p_planets[player->capitol_planet].star];
       if (player->capitol_planet >= 0 && cc_size < 10
       && cc_size < connected_component_size[pid][MAX_STARS]/3) {
       isolated = 1;
       }
       }
    */
  
    int16_t tech_path[256];
    int k = 0;
    if (aidata->high_prod_start[player_id] && !player->traits[TRAIT_EATS_MINERALS]) { tech_path[k++] = TECH_APP_FREIGHTERS; }
    tech_path[k++] = TECH_APP_ELECTRONIC_COMPUTER;
    tech_path[k++] = TECH_APP_RESEARCH_LABORATORY;
    if (advanced_construction_choice != TECH_APP_HEAVY_ARMOR) {
      tech_path[k++] = TECH_APP_REINFORCED_HULL;
    } else {
      tech_path[k++] = TECH_APP_FIGHTER_BAYS;
    }
    tech_path[k++] = advanced_construction_choice;
    if (!aidata->high_prod_start[player_id] || player->traits[TRAIT_EATS_MINERALS]) { tech_path[k++] = TECH_APP_FREIGHTERS; }
    
    tech_path[k++] = TECH_APP_STANDARD_FUEL_CELLS;

    tech_path[k++] = TECH_APP_SPACE_SCANNER;  //
    tech_path[k++] = TECH_APP_FUSION_RIFLE;   // Experimental: anti-blitz
    tech_path[k++] = TECH_APP_BATTLE_SCANNER; //

    tech_path[k++] = TECH_APP_COLONY_SHIP;
    
    tech_path[k++] = TECH_APP_BIOSPHERES;

    // TODO: better test of isolation
    if (aidata->strategy[player_id] == AITK::STRAT_EXPAND && aidata->nb_colonizable_planets[player_id] < 2) {
      tech_path[k++] = TECH_APP_DEUTERIUM_FUEL_CELLS;
    }

    int get_cloning_center = 0;
    {
      int clones_or_soil = 0;
      if (player->traits[TRAIT_EATS_MINERALS]) { clones_or_soil += 10; }
      if (player->traits[TRAIT_CYBERNETIC]) { clones_or_soil += 2; }
      if (player->traits[TRAIT_AQUATIC]) { clones_or_soil += 1; }
      if (player->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_UNIFICATION / 2) { clones_or_soil += 1; }
      if (player->traits[TRAIT_FOOD_BONUS] < 0) { clones_or_soil += player->traits[TRAIT_FOOD_BONUS]; }
      else { clones_or_soil += player->traits[TRAIT_FOOD_BONUS]/2; }
      clones_or_soil -= player->traits[TRAIT_POP_GROWTH_BONUS]/50;
      get_cloning_center = (clones_or_soil >= 2);
    }

    if (aidata->strategy[player_id] == AITK::STRAT_DROIDS) {
      // demolith & similar rush droids
      if (!get_cloning_center) {
        tech_path[k++] = TECH_APP_SOIL_ENRICHMENT;
      }
      tech_path[k++] = TECH_APP_NEURAL_SCANNER;
      tech_path[k++] = TECH_APP_PLANETARY_SUPERCOMPUTER;
      tech_path[k++] = TECH_APP_BATTLE_PODS;
      tech_path[k++] = TECH_APP_SPACEPORT;
      tech_path[k++] = TECH_APP_ROBO_MINERS;
      tech_path[k++] = TECH_APP_CYBER_SECURITY_LINK;
      tech_path[k++] = TECH_APP_AUTOLAB;
      tech_path[k++] = TECH_APP_ANDROID_WORKERS;
      if (get_cloning_center) {
        tech_path[k++] = TECH_APP_CLONING_CENTER;
      }
    } else {
      if (get_cloning_center) {
        tech_path[k++] = TECH_APP_CLONING_CENTER;
      } else {
        tech_path[k++] = TECH_APP_SOIL_ENRICHMENT;
      }
    }
  
    tech_path[k++] = TECH_APP_NEURAL_SCANNER;
    tech_path[k++] = TECH_APP_PLANETARY_SUPERCOMPUTER;

    if (player->traits[TRAIT_ENVIRONMENT_IMMUNE]) {
      tech_path[k++] = TECH_APP_TRITANIUM_ARMOR;
      tech_path[k++] = TECH_APP_MERCULITE_MISSILE;
      tech_path[k++] = TECH_APP_SPACE_ACADEMY;
      tech_path[k++] = TECH_APP_BATTLE_PODS;
      tech_path[k++] = TECH_APP_SPACEPORT;
      tech_path[k++] = TECH_APP_ROBO_MINERS;
      tech_path[k++] = TECH_APP_IRIDIUM_FUEL_CELLS;
      tech_path[k++] = TECH_APP_ZORTRIUM_ARMOR;
    } else {
      tech_path[k++] = TECH_APP_DEUTERIUM_FUEL_CELLS;
      tech_path[k++] = TECH_APP_MERCULITE_MISSILE;
      tech_path[k++] = TECH_APP_SPACE_ACADEMY;
      tech_path[k++] = TECH_APP_ATMOSPHERIC_RENEWER;
      tech_path[k++] = TECH_APP_BATTLE_PODS;
      tech_path[k++] = TECH_APP_SPACEPORT;
      tech_path[k++] = TECH_APP_ROBO_MINERS;
      tech_path[k++] = TECH_APP_ZORTRIUM_ARMOR;
    }
  
    tech_path[k++] = TECH_APP_AUGMENTED_ENGINES;
    tech_path[k++] = TECH_APP_ION_DRIVE;
  
    if (player->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_DEMOCRACY / 2) {
      tech_path[k++] = TECH_APP_CYBER_SECURITY_LINK;
      tech_path[k++] = TECH_APP_MICROBIOTICS;
      tech_path[k++] = TECH_APP_TERRAFORMING;
      tech_path[k++] = TECH_APP_AUTOLAB;
    } else {
      tech_path[k++] = TECH_APP_EMISSIONS_GUIDANCE_SYSTEM;
      tech_path[k++] = TECH_APP_AUTOLAB;
      tech_path[k++] = TECH_APP_MICROBIOTICS;
      tech_path[k++] = TECH_APP_TERRAFORMING;
    }
  
    tech_path[k++] = TECH_APP_ADVANCED_DAMAGE_CONTROL;
    tech_path[k++] = TECH_APP_TITAN_CONSTRUCTION;

    tech_path[k++] = TECH_APP_ECM_JAMMER;
    tech_path[k++] = TECH_APP_INERTIAL_STABILIZER;
    tech_path[k++] = TECH_APP_PLANETARY_RADIATION_SHIELD;

    tech_path[k++] = TECH_APP_FUSION_RIFLE;
    tech_path[k++] = TECH_APP_BATTLE_SCANNER;
    tech_path[k++] = TECH_APP_NEUTRON_SCANNER;
    if (player->traits[TRAIT_HEAVY_G_WORLD]) {
      tech_path[k++] = TECH_APP_TRACTOR_BEAM;
    } else {
      tech_path[k++] = TECH_APP_PLANETARY_GRAVITY_GENERATOR;
    }
    tech_path[k++] = TECH_APP_SUB_SPACE_COMMUNICATIONS;
    
    tech_path[k++] = TECH_APP_ALIEN_MANAGEMENT_CENTER;

    if (player->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_UNIFICATION / 2) {
      tech_path[k++] = TECH_APP_ANDROID_SCIENTISTS;
      tech_path[k++] = TECH_APP_GALACTIC_CYBERNET;
    } else {
      tech_path[k++] = TECH_APP_ANDROID_WORKERS;
      tech_path[k++] = TECH_APP_VIRTUAL_REALITY_NETWORK;
    }

    tech_path[k++] = TECH_APP_PLANETARY_STOCK_EXCHANGE;
    tech_path[k++] = TECH_APP_ASTRO_UNIVERSITY;
    if (player->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_FEUDAL / 2) {
      tech_path[k++] = TECH_APP_CONFEDERATION;
    } else if (player->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_DICTATORSHIP / 2) {
      tech_path[k++] = TECH_APP_IMPERIUM;
    } else if (player->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_DEMOCRACY / 2) {
      tech_path[k++] = TECH_APP_FEDERATION;
    } else {
      tech_path[k++] = TECH_APP_GALACTIC_UNIFICATION;
    }

    tech_path[k++] = TECH_APP_ANTI_MATTER_DRIVE;

    if (player->tech_applications[TECH_APP_CYBERTRONIC_COMPUTER] != 3
        && player->tech_applications[TECH_APP_MOLECULARTRONIC_COMPUTER] == 1) {
      tech_path[k++] = TECH_APP_MOLECULARTRONIC_COMPUTER;
    }
  
    tech_path[k++] = TECH_APP_PHASOR;
    tech_path[k++] = TECH_APP_PLASMA_RIFLE;
    tech_path[k++] = TECH_APP_DISRUPTER_CANNON;
    tech_path[k++] = TECH_APP_HYPERSPACE_COMMUNICATIONS;
  
    tech_path[k++] = TECH_APP_ACHILLES_TARGETING_UNIT;
  
    for (int i = 0; i < k; i++) {
      int16_t t = tech_path[i];
      if (player->tech_fields[TECHDATA::technology_applications[t].tech_field_id] == 2) {
        *preferred_tech = t;
        if (player->tech_applications[t] == 1) {
          player->current_research_application = t;
          player->current_research_field = TECHDATA::technology_applications[t].tech_field_id;
          return;
        } else { // Uncreative races
          int field = TECHDATA::technology_applications[t].tech_field_id;
          for (int j = 0; j < 4; j++) {
            t = TECHDATA::technology_fields[field].tech[j];
            if (t == 0) break;
            if (player->tech_applications[t] == 1) {
              player->current_research_application = t;
              player->current_research_field = TECHDATA::technology_applications[t].tech_field_id;
              return;
            }
          }
        }
      }
    }
  
    // Fall back to the old AI
    if (needs_new_tech) { 
      player->got_new_app = 1;
      aitech::AI_Tech_Select(player);
    }
    *preferred_tech = (uint8_t)player->current_research_application;
  }
  
  void __cdecl Set_Research_(struct AITK::aidata * aidata) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "aitk: Set_Research_ start");
    
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      
      Set_Research_One_Player_(aidata, pid, aidata->preferred_tech+pid);
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "aitk: Set_Research_ end");
  }

// Trade
  
static int __cdecl Player_Wants_Tech_(int player_id, int tech) {
  struct s_player * p = &MOX::player[player_id];
  if (p->traits[TRAIT_ENVIRONMENT_IMMUNE]) {
    if (tech == TECH_APP_POLLUTION_PROCESSOR
	|| tech == TECH_APP_ATMOSPHERIC_RENEWER
	|| tech == TECH_APP_NANO_DISASSEMBLERS
	|| tech == TECH_APP_CORE_WASTE_DUMPS) {
      return 0;
    }
  }
  if (p->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_UNIFICATION / 2) {
    if (tech == TECH_APP_HOLO_SIMULATOR
	|| tech == TECH_APP_VIRTUAL_REALITY_NETWORK
	|| tech == TECH_APP_PLEASURE_DOME) {
      return 0;
    }
  }
  if (p->traits[TRAIT_EATS_MINERALS]) {
    if (tech == TECH_APP_HYDROPONIC_FARM
	|| tech == TECH_APP_SOIL_ENRICHMENT
	|| tech == TECH_APP_SUBTERRANEAN_FARMS
	|| tech == TECH_APP_BIOMORPHIC_FUNGI
	|| tech == TECH_APP_FOOD_REPLICATORS
	|| tech == TECH_APP_ANDROID_FARMERS) {
      return 0;
    }
  }
  return 1;
}

  
  static int __cdecl Exchange_Techs_(int tech_a, int tech_b) {
    // TODO : trade with human players too
    int changed = 0;
    
    for(int p1 = 0; p1 < MOX::NUM_PLAYERS; p1++) {
      if (MOX::player[p1].objectives == PLAYER_OBJECTIVE_HUMAN) continue;
      if (MOX::player[p1].eliminated) continue;
      if (MOX::player[p1].traits[TRAIT_REPULSIVE]) continue;
    
      if (!(MOX::player[p1].tech_applications[tech_a] != 3
            && MOX::player[p1].tech_applications[tech_b] == 3)) continue;
      if (!(Player_Wants_Tech_(p1, tech_a))) { continue; }
    
      for(int p2 = 0; p2 < MOX::NUM_PLAYERS; p2++) {
        if (p1 == p2) continue;
        if (MOX::player[p2].objectives == PLAYER_OBJECTIVE_HUMAN) continue;
        if (MOX::player[p2].eliminated) continue;
        if (MOX::player[p2].traits[TRAIT_REPULSIVE]) continue;
        if (!MOX::player[p1].contact[p2]) continue;
        if (MOX::player[p1].treaty[p2] > 3) continue;
        if (!(Player_Wants_Tech_(p2, tech_b))) { continue; }
      
        // TODO : check if both players are interested & can learn
        if (MOX::player[p2].tech_applications[tech_b] != 3 && MOX::player[p2].tech_applications[tech_a] == 3) {
          COLCALC::Player_Gets_Tech_App(&MOX::player[p1], tech_a);
          COLCALC::Player_Gets_Tech_App(&MOX::player[p2], tech_b);
          changed = 1;
        }

      }
    }

    return changed;
  }
  
  static int __cdecl Could_Research_To_Trade_(int p1, int tech_to_research, int tech_to_obtain_by_trade) {
    if (!(MOX::player[p1].tech_applications[tech_to_research] == 1
          && MOX::player[p1].tech_fields[TECHDATA::technology_applications[tech_to_research].tech_field_id] == 2
          && Player_Wants_Tech_(p1, tech_to_research)
          && Player_Wants_Tech_(p1, tech_to_obtain_by_trade))) {
      return 0;
    }

    if (MOX::player[p1].tech_applications[tech_to_obtain_by_trade] == 3) return 1;
  
    for(int p2 = 0; p2 < MOX::NUM_PLAYERS; p2++) {
      if (p1 == p2) continue;
      if (MOX::player[p2].objectives == PLAYER_OBJECTIVE_HUMAN) continue;
      if (MOX::player[p2].eliminated) continue;
      if (MOX::player[p2].traits[TRAIT_REPULSIVE]) continue;
      if (!MOX::player[p1].contact[p2]) continue;
      if (MOX::player[p1].treaty[p2] > 3) continue;
    
      // TODO : check if both players are interested & can learn
      if(MOX::player[p2].tech_applications[tech_to_research] != 3
         && MOX::player[p2].tech_applications[tech_to_obtain_by_trade] == 3
         && Player_Wants_Tech_(p2, tech_to_research)) {
        return 1;
      } 
    
    }

    return 0;
  }

  static void __cdecl Tech_Tradeoff_(struct AITK::aidata * aidata, int tech_a, int tech_b) {
    // TODO : trade with human players too
    Exchange_Techs_(tech_a, tech_b);
  
    for(int p1 = 0; p1 < MOX::NUM_PLAYERS; p1++) {
      if (MOX::player[p1].objectives == PLAYER_OBJECTIVE_HUMAN) continue;
      if (MOX::player[p1].eliminated) continue;
      if (MOX::player[p1].traits[TRAIT_REPULSIVE]) continue;
      if (MOX::player[p1].traits[TRAIT_UNCREATIVE]) continue;
      if (MOX::player[p1].traits[TRAIT_CREATIVE]) continue;

      if (MOX::player[p1].current_research_application == tech_a
          || MOX::player[p1].current_research_application == tech_b) {
      
        int tech_1, tech_2;
        if (aidata->preferred_tech[p1] == tech_b) {
          tech_1 = tech_b; tech_2 = tech_a;
        } else {
          tech_1 = tech_a; tech_2 = tech_b;
        } 

        if (Could_Research_To_Trade_(p1, tech_2, tech_1)
            && !Could_Research_To_Trade_(p1, tech_1, tech_2)) {
          MOX::player[p1].current_research_application = tech_2;
          MOX::player[p1].current_research_field = TECHDATA::technology_applications[tech_2].tech_field_id;
        } else {
          MOX::player[p1].current_research_application = tech_1;
          MOX::player[p1].current_research_field = TECHDATA::technology_applications[tech_1].tech_field_id;
        }
      }
    
    }
  }


  static int __cdecl Could_Research_To_Trade_Both_(int p1, int tech_to_research,
                                                   int tech_target_1,
                                                   int tech_target_2) {
    if (!(MOX::player[p1].tech_applications[tech_to_research] == 1
          && MOX::player[p1].tech_fields[TECHDATA::technology_applications[tech_to_research].tech_field_id] == 2
          && Player_Wants_Tech_(p1, tech_to_research)
          && Player_Wants_Tech_(p1, tech_target_1)
          && Player_Wants_Tech_(p1, tech_target_2))) {
      return 0;
    }

    int can_get_1_from_0 = 0;
    int can_get_2_from_0 = 0;
    int can_get_2_from_1 = 0;
    int can_get_1_from_2 = 0;
  
    if (MOX::player[p1].tech_applications[tech_target_1] == 3) can_get_1_from_0 = 1;
    if (MOX::player[p1].tech_applications[tech_target_2] == 3) can_get_2_from_0 = 1;

    for(int p2 = 0; p2 < MOX::NUM_PLAYERS; p2++) {
      if (p1 == p2) continue;
      if (MOX::player[p2].objectives == PLAYER_OBJECTIVE_HUMAN) continue;
      if (MOX::player[p2].eliminated) continue;
      if (MOX::player[p2].traits[TRAIT_REPULSIVE]) continue;
      if (!MOX::player[p1].contact[p2]) continue;
      if (MOX::player[p1].treaty[p2] > 3) continue;

      int old_can_get_1_from_0 = can_get_1_from_0;
    
      // TODO : check if both players are interested & can learn
      if (MOX::player[p2].tech_applications[tech_to_research] != 3
          && MOX::player[p2].tech_applications[tech_target_1] == 3
          && Player_Wants_Tech_(p2, tech_to_research)) {
        can_get_1_from_0 = 1;
        if (can_get_2_from_0 || can_get_2_from_1) return 1;
      }
      if (MOX::player[p2].tech_applications[tech_to_research] != 3
          && MOX::player[p2].tech_applications[tech_target_2] == 3
          && Player_Wants_Tech_(p2, tech_to_research)) {
        can_get_2_from_0 = 1;
        if (old_can_get_1_from_0 || can_get_1_from_2) return 1;
      }
      if (MOX::player[p2].tech_applications[tech_target_1] != 3
          && MOX::player[p2].tech_applications[tech_target_2] == 3
          && Player_Wants_Tech_(p2, tech_target_1)) {
        can_get_2_from_1 = 1;
        if (can_get_1_from_0) return 1;
      }
      if (MOX::player[p2].tech_applications[tech_target_2] != 3
          && MOX::player[p2].tech_applications[tech_target_1] == 3
          && Player_Wants_Tech_(p2, tech_target_2)) {
        can_get_1_from_2 = 1;
        if (can_get_2_from_0) return 1;
      }
    
    }

    return 0;
  }

  static void __cdecl Tech_Tradeoff_3_(struct AITK::aidata * aidata, int tech_a, int tech_b, int tech_c) {
    // TODO : trade with human players too
    while (1) {
      int changed = 0;
      changed = Exchange_Techs_(tech_a, tech_b) || changed;
      changed = Exchange_Techs_(tech_a, tech_c) || changed;
      changed = Exchange_Techs_(tech_b, tech_c) || changed;
      if (!changed) break;
    }
  
    for(int p1 = 0; p1 < MOX::NUM_PLAYERS; p1++) {
      if (MOX::player[p1].objectives == PLAYER_OBJECTIVE_HUMAN) continue;
      if (MOX::player[p1].eliminated) continue;
      if (MOX::player[p1].traits[TRAIT_REPULSIVE]) continue;
      if (MOX::player[p1].traits[TRAIT_UNCREATIVE]) continue;
      if (MOX::player[p1].traits[TRAIT_CREATIVE]) continue;
    
      if (MOX::player[p1].current_research_application == tech_a
          || MOX::player[p1].current_research_application == tech_b
          || MOX::player[p1].current_research_application == tech_c) {

        int tech_1, tech_2, tech_3;
        if (aidata->preferred_tech[p1] == tech_c) {
          if (Player_Wants_Tech_(p1, tech_a)) {
            tech_1 = tech_c; tech_2 = tech_a; tech_3 = tech_b;
          } else {
            tech_1 = tech_c; tech_2 = tech_b; tech_3 = tech_c;
          }
        } else if (aidata->preferred_tech[p1] == tech_b) {
          if (Player_Wants_Tech_(p1, tech_a)) {
            tech_1 = tech_b; tech_2 = tech_a; tech_3 = tech_c;
          } else {
            tech_1 = tech_b; tech_2 = tech_c; tech_3 = tech_b;
          }
        } else {
          if (Player_Wants_Tech_(p1, tech_b)) {
            tech_1 = tech_a; tech_2 = tech_b; tech_3 = tech_c;
          } else {
            tech_1 = tech_a; tech_2 = tech_c; tech_3 = tech_b;
          }
        } 

        if (Could_Research_To_Trade_Both_(p1, tech_3, tech_1, tech_2)
            && !Could_Research_To_Trade_Both_(p1, tech_1, tech_2, tech_3)
            && !Could_Research_To_Trade_Both_(p1, tech_2, tech_1, tech_3)) {
          MOX::player[p1].current_research_application = tech_3;
          MOX::player[p1].current_research_field = TECHDATA::technology_applications[tech_3].tech_field_id;
        } else if (Could_Research_To_Trade_(p1, tech_2, tech_1)
                   && !Could_Research_To_Trade_(p1, tech_1, tech_2)) {
          MOX::player[p1].current_research_application = tech_2;
          MOX::player[p1].current_research_field = TECHDATA::technology_applications[tech_2].tech_field_id;
        } else {
          MOX::player[p1].current_research_application = tech_1;
          MOX::player[p1].current_research_field = TECHDATA::technology_applications[tech_1].tech_field_id;
        }

      }
    }
  }


  void __cdecl Trade_Tech_(struct AITK::aidata * aidata) {
    Tech_Tradeoff_3_(aidata, TECH_APP_AUTOMATED_FACTORIES, TECH_APP_PLANETARY_MISSILE_BASE, TECH_APP_HEAVY_ARMOR);
    Tech_Tradeoff_(aidata, TECH_APP_BIOSPHERES, TECH_APP_HYDROPONIC_FARM);
    Tech_Tradeoff_(aidata, TECH_APP_SOIL_ENRICHMENT, TECH_APP_CLONING_CENTER);
    Tech_Tradeoff_(aidata, TECH_APP_BATTLE_PODS, TECH_APP_SURVIVAL_PODS);
    Tech_Tradeoff_(aidata, TECH_APP_FIGHTER_GARRISON, TECH_APP_SPACEPORT);
    Tech_Tradeoff_3_(aidata, TECH_APP_ROBO_MINERS, TECH_APP_POWERED_ARMOR, TECH_APP_BATTLESTATION);
    Tech_Tradeoff_(aidata, TECH_APP_TRITANIUM_ARMOR, TECH_APP_DEUTERIUM_FUEL_CELLS);
    Tech_Tradeoff_3_(aidata, TECH_APP_IRIDIUM_FUEL_CELLS, TECH_APP_PULSON_MISSILE, TECH_APP_ATMOSPHERIC_RENEWER);
    Tech_Tradeoff_3_(aidata, TECH_APP_ZORTRIUM_ARMOR, TECH_APP_MICROLITE_CONSTRUCTION, TECH_APP_NANO_DISASSEMBLERS);
  
    Tech_Tradeoff_3_(aidata, TECH_APP_RESEARCH_LABORATORY, TECH_APP_DAUNTLESS_GUIDANCE_SYSTEM, TECH_APP_OPTRONIC_COMPUTER);
    Tech_Tradeoff_3_(aidata, TECH_APP_PLANETARY_SUPERCOMPUTER, TECH_APP_HOLO_SIMULATOR, TECH_APP_POSITRONIC_COMPUTER);
    Tech_Tradeoff_3_(aidata, TECH_APP_CYBER_SECURITY_LINK, TECH_APP_EMISSIONS_GUIDANCE_SYSTEM, TECH_APP_RANGEMASTER_UNIT);
    Tech_Tradeoff_3_(aidata, TECH_APP_AUTOLAB, TECH_APP_CYBERTRONIC_COMPUTER, TECH_APP_STRUCTURAL_ANALYZER);
    Tech_Tradeoff_3_(aidata, TECH_APP_MOLECULARTRONIC_COMPUTER, TECH_APP_ACHILLES_TARGETING_UNIT, TECH_APP_PLEASURE_DOME);
  
    Tech_Tradeoff_3_(aidata, TECH_APP_BATTLE_SCANNER, TECH_APP_TACHYON_COMMUNICATIONS, TECH_APP_TACHYON_SCANNER);
    Tech_Tradeoff_(aidata, TECH_APP_NEUTRON_SCANNER, TECH_APP_NEUTRON_BLASTER);
    Tech_Tradeoff_3_(aidata, TECH_APP_PLANETARY_GRAVITY_GENERATOR, TECH_APP_GRAVITON_BEAM, TECH_APP_TRACTOR_BEAM);
    Tech_Tradeoff_(aidata, TECH_APP_SUB_SPACE_COMMUNICATIONS, TECH_APP_JUMP_GATE);
    Tech_Tradeoff_3_(aidata, TECH_APP_PHASOR, TECH_APP_MULTI_PHASED_SHIELDS, TECH_APP_PHASOR_RIFLE);
    Tech_Tradeoff_3_(aidata, TECH_APP_PLASMA_RIFLE, TECH_APP_PLASMA_CANNON, TECH_APP_PLASMA_WEB);
    Tech_Tradeoff_3_(aidata, TECH_APP_HYPERSPACE_COMMUNICATIONS, TECH_APP_MAULER_DEVICE, TECH_APP_SENSORS);
  
    Tech_Tradeoff_3_(aidata, TECH_APP_INERTIAL_STABILIZER, TECH_APP_ANTI_GRAV_HARNESS, TECH_APP_GYRO_DESTABILIZER);
    Tech_Tradeoff_3_(aidata, TECH_APP_PLANETARY_RADIATION_SHIELD, TECH_APP_CLASS_III_SHIELD, TECH_APP_WARP_DISSIPATER);
  
    Tech_Tradeoff_3_(aidata, TECH_APP_AUGMENTED_ENGINES, TECH_APP_FUSION_DRIVE, TECH_APP_FUSION_BOMB);
    Tech_Tradeoff_3_(aidata, TECH_APP_SHIELD_CAPACITORS, TECH_APP_ION_DRIVE, TECH_APP_ION_PULSE_CANNON);
    Tech_Tradeoff_3_(aidata, TECH_APP_ANTI_MATTER_DRIVE, TECH_APP_ANTI_MATTER_BOMB, TECH_APP_ANTI_MATTER_TORPEDOES);

    Tech_Tradeoff_3_(aidata, TECH_APP_ADVANCED_DAMAGE_CONTROL, TECH_APP_ASSAULT_SHUTTLES, TECH_APP_FAST_MISSILE_RACKS);
    Tech_Tradeoff_3_(aidata, TECH_APP_TITAN_CONSTRUCTION, TECH_APP_GROUND_BATTERIES, TECH_APP_BATTLEOIDS);
  }
  
}
