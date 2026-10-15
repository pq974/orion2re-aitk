#include "pch.h"

#include "game/aitk_common.h"

namespace AITK {

  int cmp_int_pair_dec(const void * a, const void* b) {
    const struct int_pair * _a = reinterpret_cast<const struct int_pair *>(a);
    const struct int_pair * _b = reinterpret_cast<const struct int_pair *>(b);
    if (_a->x != !_b->x) {
      return _b->x - _a->x;
    } else {
      return _b->y - _a->y;
    }
  }
    
  int cmp_int_pair_inc(const void * a, const void* b) {
    const struct int_pair * _a = reinterpret_cast<const struct int_pair *>(a);
    const struct int_pair * _b = reinterpret_cast<const struct int_pair *>(b);
    if (_a->x != !_b->x) {
      return _a->x - _b->x;
    } else {
      return _a->y - _b->y;
    }
  }
  
  int __cdecl Planet_Score_(struct s_planet_data * pl, int player_id, int ignore_location) {
    struct s_player * p = &MOX::player[player_id];
    struct s_star_data * st = &MOX::star[pl->star_index];
    int pop_score = COLCALC::Planet_Max_Population_For_Player(pl, player_id);
    int minerals_score = pl->mineral_class + 2;
    int science_score = (pl->planet_special == SYSTEM_SPECIAL_ANCIENT_ARTIFACTS) ? 3 : 2;
    int gravity_score = 4;
    if (p->tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] != 3) {
      if (pl->gravity_class == PLANET_GRAVITY_LOW_G && !p->traits[TRAIT_LOW_G_WORLD]) { gravity_score = 3; }
      else if (pl->gravity_class == PLANET_GRAVITY_NORMAL_G && p->traits[TRAIT_LOW_G_WORLD]) { gravity_score = 3; }
      else if (pl->gravity_class == PLANET_GRAVITY_HEAVY_G && !p->traits[TRAIT_HEAVY_G_WORLD]) { gravity_score = 2; }
    }
    int location_score = 4;
    int distance_score = 10;
    if (!ignore_location) {
      int has_enemy_colony = 0;
      int has_pop_sink = 0;
      int building_colony_base = 0;
      int has_my_colony = 0;
      if (st->has_colony & ~(1<<player_id)) { // (means colony or outpost)
        for (int orbit = 0; orbit < 5; orbit++) {
          int this_plid = st->planet_index[orbit];
          struct s_planet_data * this_pl;
          if (this_plid >= 0) { this_pl = &MOX::planet[this_plid]; }
          else { continue; }
          if (this_pl->planet_type == PLANET_TYPE_PLANET
              && this_pl->colony_index >= 0) {
            struct s_colony * c = &MOX::colony[this_pl->colony_index];
            if (!c->outpost_flag && c->owner != player_id) {
              has_enemy_colony = 1;
            }
          }
        }
      }
      if (st->has_colony & (1<<player_id)) {
        for (int orbit = 0; orbit < 5; orbit++) {
          int this_plid = st->planet_index[orbit];
          struct s_planet_data * this_pl;
          if (this_plid >= 0) { this_pl = &MOX::planet[this_plid]; }
          else { continue; }
          if (this_pl->planet_type == PLANET_TYPE_PLANET
              && this_pl->colony_index >= 0) {
            struct s_colony * c = &MOX::colony[this_pl->colony_index];
            if (!c->outpost_flag && c->owner == player_id) {
              has_my_colony = 1;
              if (COLCALC::Planet_Max_Population_For_Player(this_pl, player_id)
                  >= 3*COLCALC::Size_And_Climate_Max_Population(PLANET_SIZE_MEDIUM, PLANET_CLIMATE_TERRAN, p->traits[TRAIT_ENVIRONMENT_IMMUNE])/4) {
                has_pop_sink = 1;
              }
              if (c->producing[0] == BUILDING_COLONY_BASE) {
                building_colony_base = 1;
                break;
              }
            }
          }
        }
      }
      if (building_colony_base) {
        location_score = 1;
      } else if (has_pop_sink) {
        location_score = 6;
      } else if (st->has_colony & (1<<player_id)) {
        if (has_my_colony && !(AIDATA::star_in_extended_range_of_player[player_id][pl->star_index] & ~(1 << player_id))) {
          location_score = 2;
        }
      } else {
        // TODO : resore chokepoint-awareness
        //if (cut_vertices[player_id][pl->star]) {
        //  location_score = 8;
        //} else if (has_enemy_colony) {
          location_score = 2;
        //}
      }
      
      /* TODO : compute distance to the core empire instead */
      if (p->home_planet_id >= 0) {
        int eta = SETTLER::Settler_ETA(p, MOX::planet[p->home_planet_id].star_index, pl->star_index);
        distance_score = 50 / (eta+4);
        if (distance_score < 1) { distance_score = 1; }
      }
    }

    // TODO: change location score if reachable by a threat
    return pop_score * minerals_score * science_score * gravity_score * location_score * distance_score;
  }

  void __cdecl Init_AI_Data_(struct AITK::aidata * aidata) {
    aidata->game_turn = MOX::stardate-35000;

    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      if (p->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_DEMOCRACY / 2
          && (p->traits[TRAIT_ANCIENT_HOME_WORLD] || p->traits[TRAIT_RESEARCH_BONUS] > 0)
          && p->tech_fields[TECHDATA::technology_applications[TECH_APP_ANDROID_WORKERS].tech_field_id] != TECH_RESEARCH_STATUS_RESEARCHED) {
        aidata->strategy[pid] = STRAT_DROIDS;
      } else {
        aidata->strategy[pid] = STRAT_EXPAND;
      }
    }

    if (aidata->game_turn == 0) {
      for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
        struct s_player * p = MOX::player + pid;
        if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
        // TODO: make this depend on strategy
        p->objectives = PLAYER_OBJECTIVE_EXPANSIONIST;
        p->personality = PLAYER_PERSONALITY_XENOPHOBIC;
      }
    }
    
    /* DEBUG
    if (aidata->game_turn == 0) {
      struct s_player * saved_p_ai = AIMOVE::p_ai;
      for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
        struct s_player * p = MOX::player + pid;
        if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
        for (int i = 0; i < sizeof(p->traits); i++) {
          p->traits[i] = 0;
        }
        p->traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_DEMOCRACY;
        p->traits[TRAIT_EATS_MINERALS] = 1;
        p->traits[TRAIT_ANCIENT_HOME_WORLD] = 1;
        p->traits[TRAIT_REPULSIVE] = 1;
        for (int cid = 0; cid < MOX::NUM_COLONIES; cid++) {
          struct s_colony * c = MOX::colony+cid;
          if (c->owner != pid) { continue; }
          if (c->outpost_flag) { continue; }
          MOX::planet[c->planet].planet_special = SYSTEM_SPECIAL_ANCIENT_ARTIFACTS;
          MOX::planet[c->planet].gravity_class = PLANET_GRAVITY_NORMAL_G;
          MOX::planet[c->planet].size = PLANET_SIZE_MEDIUM;
          MOX::planet[c->planet].climate = PLANET_CLIMATE_TERRAN;
          MOX::planet[c->planet].mineral_class = MINERAL_RESOURCE_ABUNDANT;
          c->buildings[BUILDING_CAPITOL] = 1;
        }
      }
    }
    */
    
    for (int i = 0; i < 8; i++) {
      aidata->nb_colonies_by_player[i] = 0;
      aidata->nb_unblockaded_colonies_by_player[i] = 0;
      aidata->nb_blockaded_colonies_by_player[i] = 0;
      aidata->population[i] = MOX::player[i].settlers_freighted;
      aidata->nb_queued_spies[i] = 0;
    }
    for (int i = 0; i < MOX::NUM_COLONIES; i++) {
      struct s_colony * c = MOX::colony+i;
      if (c->owner == -1) { continue; }
      if (c->outpost_flag) { continue; }
      int pid = c->owner;
      struct s_player * p = MOX::player + pid;
      if (p->eliminated) { continue; }
      aidata->colonies_by_player[pid][aidata->nb_colonies_by_player[pid]] = i;
      aidata->nb_colonies_by_player[pid]++;
      if ((MOX::star[MOX::planet[c->planet].star_index].blockaded & (1<<pid)) == 0) {
        aidata->unblockaded_colonies_by_player[pid][aidata->nb_unblockaded_colonies_by_player[pid]] = i;
        aidata->nb_unblockaded_colonies_by_player[pid]++;
      } else {
        aidata->blockaded_colonies_by_player[pid][aidata->nb_blockaded_colonies_by_player[pid]] = i;
        aidata->nb_blockaded_colonies_by_player[pid]++;          
      }
      aidata->population[pid] += c->n_pops;
      if (c->producing[0] == COLONY_PRODUCTION_SPY) {
        aidata->nb_queued_spies[pid]++;
      }
    }

    for (int pid = 0; pid < 8; pid++) {
      aidata->nb_stars_by_player[pid] = 0;
      aidata->nb_threats[pid] = 0;
      aidata->nb_threatens[pid] = 0;
      for (int pid2 = 0; pid2 < 8; pid2++) {
        aidata->threatens[pid2][pid] = 0;
      }
    }
    for (int stid = 0; stid < MOX::NUM_STARS; stid++) {
      struct s_star_data * st = MOX::star+stid;
      if (st->spectral_class == STAR_CLASS_BLACK_HOLE) { continue; }
      for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
        aidata->no_shipping_at_star[pid][stid] = 0;
        if (st->has_colony & (1 << pid)) {
          aidata->stars_by_player[pid][aidata->nb_stars_by_player[pid]] = stid;
          aidata->nb_stars_by_player[pid]++;
          for (int pid2 = 0; pid2 < 8; pid2++) {
            if (pid2 == pid) { continue; }
            if (aidata->threatens[pid2][pid]) { continue; }
            if ((*AIDATA::star_in_range_of_player)[stid] & (1 << pid2)) {
              aidata->threatens[pid2][pid] = 1;
              aidata->nb_threats[pid]++;
              aidata->nb_threatens[pid2]++;
            }
          }
        }
      }
    }

    for (int i = 0; i < 8; i++) {
      aidata->nb_colony_ships[i] = 0;
      aidata->nb_queued_colony_ships[i] = 0;
      aidata->nb_outpost_ships[i] = 0;
      aidata->nb_queued_outpost_ships[i] = 0;
      aidata->nb_transports[i] = 0;
      aidata->nb_queued_transports[i] = 0;
      aidata->combat_command_points[i] = 0;
      aidata->queued_combat_command_points[i] = 0;
    }
    for (int ship_id = 0; ship_id < MOX::NUM_SHIPS; ship_id++) {
      struct s_ship_data * sh = &MOX::ship[ship_id];
      if (sh->owner < 0 || sh->owner >= MOX::NUM_PLAYERS || sh->status == 5) { continue; }
      int pid = sh->owner;
      
      if (sh->d.ship_type == SHIP_TYPE_COLONY) {
        if (sh->status == 4 || sh->status == 6) {
          aidata->nb_queued_colony_ships[pid]++;
        } else {
          //aidata->colony_ships[aidata->nb_colony_ships] = ship_id;
          aidata->nb_colony_ships[pid]++;
        }
      } else if  (sh->d.ship_type == SHIP_TYPE_OUTPOST) {
        if (sh->status == 4 || sh->status == 6) {
          aidata->nb_queued_outpost_ships[pid]++;
        } else {
          //aidata->outpost_ships[aidata->nb_outpost_ships] = ship_id;
          aidata->nb_outpost_ships[pid]++;
        }
      } else if  (sh->d.ship_type == SHIP_TYPE_TRANSPORT) {
        if (sh->status == 4 || sh->status == 6) {
          aidata->nb_queued_transports[pid]++;
        } else {
          aidata->nb_transports[pid]++;
        }
      } else if  (sh->d.ship_type == SHIP_TYPE_COMBAT && sh->d.size > HULL_FRIGATE) { // TODO: exclude only scouts, not all frigates ?
        if (sh->status == 4 || sh->status == 6) {
          aidata->queued_combat_command_points[pid] += sh->d.size + 1;
        } else {
          aidata->combat_command_points[pid] += sh->d.size + 1;
        }
      }
    }
      
    int saved_i_ai = AIMOVE::i_ai;
    struct s_player * saved_p_ai = AIMOVE::p_ai;
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      AIMOVE::i_ai = pid;
      AIMOVE::p_ai = &MOX::player[pid];
      int16_t nb_colonizable_planets;
      AIMOVE::Get_Colonizable_Planets(aidata->colonizable_planets[pid], &nb_colonizable_planets);
      // Remove guarded planets until we have zortrium
      // TODO: find a better condition
      if (p->tech_fields[TECHDATA::technology_applications[TECH_APP_ZORTRIUM_ARMOR].tech_field_id] != 3) {
        for (int i = 0; i < nb_colonizable_planets; i++) {
          int plid = aidata->colonizable_planets[pid][i].planet_idx;
          struct s_planet_data * pl = &MOX::planet[plid];
          if (AIMOVE::Star_Has_Monster(pl->star_index)) {
            memmove(&(aidata->colonizable_planets[pid][i]), &(aidata->colonizable_planets[pid][nb_colonizable_planets-1]), sizeof(aidata->colonizable_planets[pid][i]));
            nb_colonizable_planets--;
            i--;
          }
        }
      }
      aidata->nb_colonizable_planets[pid] = nb_colonizable_planets;
    }
    AIMOVE::i_ai = saved_i_ai;
    AIMOVE::p_ai = saved_p_ai;

    
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      aidata->military_ship_small[pid] = HULL_CRUISER;
      aidata->military_ship_big[pid] = HULL_BATTLESHIP;
      if (p->tech_applications[TECH_APP_TITAN_CONSTRUCTION] == TECH_RESEARCH_STATUS_RESEARCHED) {
        aidata->military_ship_small[pid] = aidata->military_ship_big[pid];
        aidata->military_ship_big[pid] = HULL_TITAN;
      }
      aidata->doom_star_design[pid].cost = 0;
      if (p->tech_applications[TECH_APP_DOOM_STAR_CONSTRUCTION] == TECH_RESEARCH_STATUS_RESEARCHED) {
        aidata->military_ship_small[pid] = aidata->military_ship_big[pid];
        aidata->military_ship_big[pid] = HULL_DOOM_STAR;
        INITSHIP::Load_Doom_Star_Ship_Design(pid, &aidata->doom_star_design[pid], MOX::settings.strategic_combat_flag != 0);
      }
    }

    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated) { continue; }
      aidata->nb_contacts[pid] = 0;
      aidata->at_risk_of_blitz[pid] = 0;
      aidata->at_risk_of_fighters_blitz[pid] = 0;
      for (int pid2 = 0; pid2 < MOX::NUM_PLAYERS; pid2++) {
        if (pid2 == pid) { continue; }
        struct s_player * p2 = MOX::player + pid2;
        if (!p2->eliminated && p->contact[pid2]) {
          aidata->nb_contacts[pid]++;
          if (aidata->game_turn < 100 && (p2->traits[TRAIT_TELEPATHIC] || p2->traits[TRAIT_WARLORD])) {
            aidata->at_risk_of_blitz[pid] = 1;
            if (p2->tech_applications[TECH_APP_FIGHTER_BAYS] == TECH_RESEARCH_STATUS_RESEARCHED) {
              if (p->tech_fields[TECHDATA::technology_applications[TECH_APP_REINFORCED_HULL].tech_field_id] != 3
                  || p->tech_fields[TECHDATA::technology_applications[TECH_APP_TRITANIUM_ARMOR].tech_field_id] != 3
                  || p->tech_fields[TECHDATA::technology_applications[TECH_APP_FUSION_BEAM].tech_field_id] != 3
                  || p->tech_fields[TECHDATA::technology_applications[TECH_APP_BATTLE_SCANNER].tech_field_id] != 3
                  || p->tech_fields[TECHDATA::technology_applications[TECH_APP_STANDARD_FUEL_CELLS].tech_field_id] != 3) {
                aidata->at_risk_of_fighters_blitz[pid] = 1;
              } else if (aidata->combat_command_points[pid] < 3 && aidata->game_turn < 60) {
                aidata->at_risk_of_fighters_blitz[pid] = 2;
              }
            }
          }
        }
      }
    }
    
    for (int pid1 = 0; pid1 < MOX::NUM_PLAYERS; pid1++) {
      aidata->nb_spies[pid1] = 0;
      for (int pid2 = 0; pid2 < MOX::NUM_PLAYERS; pid2++) {
        aidata->nb_spies[pid1] += MOX::player[pid1].spies[pid2];
      }
    }

    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) { // Production that can be bought at 1:2
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      aidata->buy_capacity[pid] = (p->bc + p->surplus_bc - 100) / 2;
      if (aidata->buy_capacity[pid] > p->bc / 2) { aidata->buy_capacity[pid] = p->bc / 2; }
      if (p->surplus_bc < 0) { aidata->buy_capacity[pid] -= p->surplus_bc*5; }
    }

    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      aidata->high_prod_start[pid] = 0;
    }

    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      aidata->queued_command_points[pid] = 0;
    }
    for (int shid = 0; shid < MOX::NUM_SHIPS; shid++) {
      struct s_ship_data * sh = &MOX::ship[shid];
      if (sh->owner < 0 || sh->owner >= MOX::NUM_PLAYERS) {
        continue;
      }
      if (sh->status == 4 || sh->status == 6) {
        aidata->queued_command_points[sh->owner] += sh->d.size;
      }
    }
  }

  void __cdecl Uninit_AI_Data_(struct AITK::aidata * aidata) {
  }

}
