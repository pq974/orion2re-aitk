#include "pch.h"

#include "game/aitk_common.h"

namespace AITK_COLONY {

  static void __cdecl Move_All_Colonists_To_industry_() {
    for (int i = 0; i < MOX::NUM_COLONIES; i++) {
      struct s_colony * c = MOX::colony+i;
      if (c->owner == -1) { continue; }
      if (c->outpost_flag) { continue; }
      if (EVENTS::Event_Check_Space_Anomaly(i)) { continue; }
      struct s_player * p = MOX::player + c->owner;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      
      for (int j = 0; j < c->n_pops; j++) {
        if (POP::Get_Race(c->pop[j]) < 8) {
          POP::Set_Prof(c->pop+j, POP::PROF_WORKER);
        }
      }
    }
    COLCALC::All_Colony_Calculations();
  }

  struct pop_with_score {
    int colony_id;
    int pop_id;
    int score;
  };

  static int cmp_pop_with_score_dec(const void * a, const void* b) {
    return reinterpret_cast<const struct pop_with_score *>(b)->score
      - reinterpret_cast<const struct pop_with_score *>(a)->score;
  }

  static int cmp_colony_industry_inc(const void* a, const void* b) {
    struct s_colony * _a = &MOX::colony[*reinterpret_cast<const int *>(a)]; 
    struct s_colony * _b = &MOX::colony[*reinterpret_cast<const int *>(b)];
    int ind_a = _a->production[ECON_INDUSTRY] - _a->maintenance[ECON_INDUSTRY];
    int ind_b = _b->production[ECON_INDUSTRY] - _b->maintenance[ECON_INDUSTRY];
    return ind_a - ind_b;
  }
  
  static int cmp_colony_stored_industry_inc(const void* a, const void* b) {
    struct s_colony * _a = &MOX::colony[*reinterpret_cast<const int *>(a)]; 
    struct s_colony * _b = &MOX::colony[*reinterpret_cast<const int *>(b)];
    int ind_a = _a->production_spent + _a->production[ECON_INDUSTRY] - _a->maintenance[ECON_INDUSTRY];
    int ind_b = _b->production_spent + _b->production[ECON_INDUSTRY] - _b->maintenance[ECON_INDUSTRY];
    return ind_a - ind_b;
  }

  
  int __cdecl Is_Priority_Buy_(int b) {
    return b == BUILDING_ALIEN_CONTROL_CENTER
      || b == BUILDING_MARINE_BARRACKS
      || b == BUILDING_GRAVITY_GENERATOR
      || b == BUILDING_AUTOMATED_FACTORY
      || b == BUILDING_ROBO_MINER_PLANT
      || b == BUILDING_ROBOTIC_FACTORY;
  }
    
  int __cdecl Is_Kickstart_Build_(int b) {
    return Is_Priority_Buy_(b)
      || b == BUILDING_RESEARCH_LAB
      || b == BUILDING_SUPERCOMPUTER
      || b == BUILDING_AUTOLAB
      || b == BUILDING_MISSILE_BASE
      || b == BUILDING_FIGHTER_GARRISON
      || b == BUILDING_GROUND_BATTERIES
      || b == BUILDING_CLONING_CENTER;
  }
  
  int __cdecl Is_Droid_(int b) {
    return b == COLONY_PRODUCTION_WORKER
      || b == COLONY_PRODUCTION_FARMER
      || b == COLONY_PRODUCTION_SCIENTIST;
  }
  
  static void __cdecl Score_Farmer_Candidate_(struct pop_with_score * dest, int colony_id, int pop_id) {
    struct s_colony * c = MOX::colony+colony_id;
    dest->colony_id = colony_id;
    dest->pop_id = pop_id;
    dest->score = COLCALC::Colony_Pop_Prod_Produced(c, c->pop+pop_id, ECON_FOOD) * 2;
    int worker_score = COLCALC::Colony_Pop_Prod_Produced(c, c->pop+pop_id, ECON_INDUSTRY);
    int scientist_score = COLCALC::Colony_Pop_Prod_Produced(c, c->pop+pop_id, ECON_RESEARCH);
    if (worker_score >= scientist_score) {
      dest->score -= worker_score;
    } else {
      dest->score -= scientist_score;
    }
    if (Is_Kickstart_Build_(c->producing[0]) || c->producing[0] == COLONY_PRODUCTION_HOUSING) {
      dest->score -= 4*16;
    }
  }

  static void __cdecl Score_Scientist_Candidate_(struct pop_with_score * dest, int colony_id, int pop_id) {
    struct s_colony * c = MOX::colony+colony_id;
    dest->colony_id = colony_id;
    dest->pop_id = pop_id;
    dest->score = COLCALC::Colony_Pop_Prod_Produced(c, c->pop+pop_id, ECON_RESEARCH) * 2
      - COLCALC::Colony_Pop_Prod_Produced(c, c->pop+pop_id, ECON_INDUSTRY);
    if (Is_Kickstart_Build_(c->producing[0]) || c->producing[0] == COLONY_PRODUCTION_HOUSING) {
      dest->score -= 4*16;
    }
  }
  
  static void __cdecl Unblockaded_Farming_(struct AITK::aidata * aidata) {
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      
      int food_produced = 0;
      int food_needed = 0;
      int food_exported = 0;
      int available_freighters = p->n_freighters - p->settlers_freighted * 5;
      
      struct pop_with_score candidates[aidata->nb_unblockaded_colonies_by_player[pid]*MAX_COLONY_POPULATION];
      int nb_candidates = 0;
      
      for (int i = 0; i < aidata->nb_unblockaded_colonies_by_player[pid]; i++) {
        int cid = aidata->unblockaded_colonies_by_player[pid][i];
        if (EVENTS::Event_Check_Space_Anomaly(cid)) { continue; }
        struct s_colony * c = MOX::colony+cid;
        COLCALC::Colony_Food_Production(c, nullptr);
        COLCALC::Colony_Food_Maintenance(c); // Recompute without replicators
        
        food_needed += c->maintenance[ECON_FOOD];
        
        int food_surplus_here = c->production[ECON_FOOD] - c->maintenance[ECON_FOOD];
        if (food_surplus_here < 0) { food_surplus_here = 0; }
        int food_wasted = 0;
        if (food_exported + food_surplus_here > available_freighters) {
          food_wasted = food_exported + food_surplus_here - available_freighters;
          food_exported = available_freighters;
        } else {
          food_exported += food_surplus_here;
        }
        food_produced += c->production[ECON_FOOD] - food_wasted;
        
        if (COLCALC::Colony_Food2_Per_Farmer(c, pid, nullptr) > 0) {
          for (int pop_id = 0; pop_id < c->n_pops; pop_id++) {
            if (POP::Get_Race(c->pop[pop_id]) >= 8) { continue; }
            Score_Farmer_Candidate_(candidates + nb_candidates, cid, pop_id);
            nb_candidates++;
          }
        }
      }
      qsort(candidates, nb_candidates, sizeof(*candidates), cmp_pop_with_score_dec);
      
      for (int candidate_id = 0; candidate_id < nb_candidates; candidate_id++) {
        if (food_produced >= food_needed) { break; }
        
        struct pop_with_score * cand = candidates + candidate_id;
        struct s_colony * c = MOX::colony + cand->colony_id;
        
        if (food_exported >= available_freighters && c->production[ECON_FOOD] >= c->maintenance[ECON_FOOD]) { continue; }
        
        int food_here_before = c->production[ECON_FOOD];
        int food_surplus_here_before = c->production[ECON_FOOD] - c->maintenance[ECON_FOOD];
        if (food_surplus_here_before < 0) { food_surplus_here_before = 0; }
        
        POP::Set_Prof(c->pop + cand->pop_id, POP::PROF_FARMER);
        if (c->maintenance[ECON_INDUSTRY] > 0) {
          COLCALC::Colony_Industry_Production(c, nullptr);
          if (3*c->production[ECON_INDUSTRY] < 4*c->maintenance[ECON_INDUSTRY]) {
            POP::Set_Prof(c->pop + cand->pop_id, POP::PROF_WORKER);
            COLCALC::Colony_Industry_Production(c, nullptr);
            continue;
          }
        }
        COLCALC::Colony_Food_Production(c, nullptr);
        
        int food_here_after = c->production[ECON_FOOD];
        int food_surplus_here_after = c->production[ECON_FOOD] - c->maintenance[ECON_FOOD];
        if (food_surplus_here_after < 0) { food_surplus_here_after = 0; }
        int food_wasted = 0;
        if (food_exported - food_surplus_here_before + food_surplus_here_after > available_freighters) {
          food_wasted = food_exported - food_surplus_here_before + food_surplus_here_after - available_freighters;
          food_exported = available_freighters;
        } else {
          food_exported += food_surplus_here_after - food_surplus_here_before;
        }
        food_produced += food_here_after - food_here_before - food_wasted;
      }
    }
    
    COLCALC::All_Colony_Calculations();
  }

  static void __cdecl Blockaded_Farming_(struct AITK::aidata * aidata) {
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      
      for (int i = 0; i < aidata->nb_blockaded_colonies_by_player[pid]; i++) {
        
        struct pop_with_score candidates[MAX_COLONY_POPULATION];
        int nb_candidates = 0;
        
        int cid = aidata->blockaded_colonies_by_player[pid][i];
        if (EVENTS::Event_Check_Space_Anomaly(cid)) { continue; }
        struct s_colony * c = MOX::colony+cid;
        if (COLCALC::Colony_Food2_Per_Farmer(c, pid, nullptr) <= 0) { continue; }
        
        COLCALC::Colony_Food_Production(c, nullptr);
        COLCALC::Colony_Food_Maintenance(c); // Recompute without replicators
        
        for (int pop_id = 0; pop_id < c->n_pops; pop_id++) {
          if (POP::Get_Race(c->pop[pop_id]) >= 8) { continue; }
          Score_Farmer_Candidate_(candidates + nb_candidates, cid, pop_id);
          nb_candidates++;
        }
        qsort(candidates, nb_candidates, sizeof(*candidates), cmp_pop_with_score_dec);
        
        for (int candidate_id = 0; candidate_id < nb_candidates; candidate_id++) {
          if (c->production[ECON_FOOD] >= c->maintenance[ECON_FOOD]) { break; }
          struct pop_with_score * cand = candidates + candidate_id;
          POP::Set_Prof(c->pop + cand->pop_id, POP::PROF_FARMER);
          COLCALC::Colony_Food_Production(c, nullptr);
        }
      }
    }
    
    COLCALC::All_Colony_Calculations();
  }

  int Colony_Can_Grow(struct s_colony * c) {
    int race_pop_count[10] = {0};
    for (int i = 0; i < c->n_pops; i++) {
      int race = POP::Get_Race(c->pop[i]);
      if (race < 10) {
        race_pop_count[race]++;
      }
    }
    for (int race = 0; race < MOX::NUM_PLAYERS; race++) {
      if (race_pop_count[race] == 0) continue;
      if (c->n_pops < COLCALC::Colony_Race_Pop_Limit(c, race)) { return 1; }
    }
    return 0;
  }

  void __cdecl Choose_Buildings_(struct AITK::aidata * aidata) {
    for (int cid = 0; cid < MOX::NUM_COLONIES; cid++) {
      struct s_colony * c = MOX::colony+cid;
      if (c->owner == -1) { continue; }
      if (c->outpost_flag) { continue; }
      if (EVENTS::Event_Check_Space_Anomaly(cid)) { continue; }
      int pid = c->owner;
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }

      int can_grow = Colony_Can_Grow(c);
      int has_farmers = 0;
      for (int pop = 0; pop < c->n_pops; pop++) {
        if (POP::Get_Prof(c->pop[pop]) == POP::PROF_FARMER) {
          has_farmers = 1;
          break;
        }
      }

      int has_pop_source = 0;
      int stid = MOX::planet[c->planet].star_index;
      struct s_star_data * st = MOX::star+stid;
      for (int orbit = 0; orbit < 5; orbit++) {
        int plid = st->planet_index[orbit];
        struct s_planet_data * pl;
        if (plid >= 0) { pl = &MOX::planet[plid]; }
        else { continue; }
        if (pl->planet_type == PLANET_TYPE_PLANET
            && pl->colony_index >= 0) {
          struct s_colony * c2 = &MOX::colony[pl->colony_index];
          if (!c2->outpost_flag && c2->owner == pid) {
            if (c2->n_pops >= 5) {
              has_pop_source = 1;
            }
          }
        }
      }

      int colony_threatened = ((*AIDATA::star_in_extended_range_of_player)[MOX::planet[c->planet].star_index] & (~(1 << c->owner))) != 0;

      int prod_10_turns = c->production_spent + 10*std::max(0, c->production[ECON_INDUSTRY]-c->maintenance[ECON_INDUSTRY]);
      int prod_5_turns = c->production_spent + 5*std::max(0, c->production[ECON_INDUSTRY]-c->maintenance[ECON_INDUSTRY]);

      if (c->producing[0] == BUILDING_NO_BUILDING || c->producing[0] == COLONY_PRODUCTION_NONE
          || c->producing[0] == COLONY_PRODUCTION_TRADE_GOODS || c->producing[0] == COLONY_PRODUCTION_HOUSING
          || c->producing[0] == COLONY_PRODUCTION_AI_STOCKPILE
          || COLCALC::Colony_Can_Build_Product(c, c->producing[0]) != 1) {
        c->producing[0] = COLONY_PRODUCTION_AI_STOCKPILE;
        
        int build_order[] = {
          BUILDING_ALIEN_CONTROL_CENTER, BUILDING_AUTOMATED_FACTORY,
          COLCALC::barracks_morale_lookup[p->traits[TRAIT_CURRENT_GOVERNMENT]] > 0 ? BUILDING_MARINE_BARRACKS : (int) COLONY_PRODUCTION_NONE,
          c->n_pops < 5 && !p->traits[TRAIT_ENVIRONMENT_IMMUNE] && SETTLER::Room_For_Another(c, POP::RACE_ANDROID, 1) ? COLONY_PRODUCTION_WORKER : COLONY_PRODUCTION_NONE,
          BUILDING_ROBO_MINER_PLANT, BUILDING_ROBOTIC_FACTORY, BUILDING_ROBOTIC_FACTORY,
          BUILDING_RESEARCH_LAB,
          can_grow && c->n_pops < 5 && !has_pop_source ? COLONY_PRODUCTION_HOUSING : (int) COLONY_PRODUCTION_NONE,
          colony_threatened ? BUILDING_MISSILE_BASE : (int) COLONY_PRODUCTION_NONE,
          colony_threatened ? BUILDING_GROUND_BATTERIES : (int) COLONY_PRODUCTION_NONE,
          colony_threatened ? BUILDING_FIGHTER_GARRISON : (int) COLONY_PRODUCTION_NONE,
          prod_10_turns >= COLCALC::Colony_Product_Cost(c, BUILDING_STAR_BASE) ? BUILDING_STAR_BASE : (int) COLONY_PRODUCTION_NONE,
          !p->traits[TRAIT_ENVIRONMENT_IMMUNE] && SETTLER::Room_For_Another(c, POP::RACE_ANDROID, 1) ? COLONY_PRODUCTION_WORKER : COLONY_PRODUCTION_NONE,
          BUILDING_SUPERCOMPUTER, BUILDING_AUTOLAB, BUILDING_GALACTIC_CYBERNET,
          can_grow ? BUILDING_CLONING_CENTER : (int) COLONY_PRODUCTION_NONE,
          can_grow && c->n_pops < 5 ? COLONY_PRODUCTION_HOUSING : (int) COLONY_PRODUCTION_NONE,
          !can_grow ? BUILDING_BIOSPHERES : (int) COLONY_PRODUCTION_NONE,
          BUILDING_GRAVITY_GENERATOR, BUILDING_HOLO_SIMULATOR, BUILDING_PLEASURE_DOME,
          has_farmers ? BUILDING_SOIL_ENRICHMENT : (int) COLONY_PRODUCTION_NONE,
          has_farmers ? BUILDING_WEATHER_CONTROLLER : (int) COLONY_PRODUCTION_NONE,
          c->pollution > 5 ? BUILDING_POLLUTION_PROCESSOR : (int) COLONY_PRODUCTION_NONE,
          c->pollution > 5 ? BUILDING_ATMOSPHERE_RENEWER : (int) COLONY_PRODUCTION_NONE,
          BUILDING_DEEP_CORE_MINE,
          BUILDING_RECYCLOTRON, BUILDING_ASTRO_UNIVERSITY,
          BUILDING_HYDROPONIC_FARM, BUILDING_SUBTERRANEAN_FARMS,
          BUILDING_RADIATION_SHIELD,
          BUILDING_SPACEPORT, BUILDING_STOCK_EXCHANGE, BUILDING_CURRENCY_EXCHANGE,
          BUILDING_ARTIFICIAL_PLANET, BUILDING_TERRAFORMING, BUILDING_GAIA_TRANSFORMATION,
        };

        for (int i = 0; i < sizeof(build_order)/sizeof(*build_order); i++) {
          int b = build_order[i];
          if (b == COLONY_PRODUCTION_NONE) { continue; }
          if (COLCALC::Colony_Can_Build_Product(c, b) == 1) { 
            c->producing[0] = b;
            break;
          }
        }
      }
    }
  }

  static void __cdecl Make_Research_Colony_(int cid, int preserve_prod) {
    struct s_colony * c = MOX::colony+cid;
    int had_pollution = c->pollution > 0;

    struct pop_with_score candidates[MAX_COLONY_POPULATION];
    int nb_candidates = 0;
        
    for (int pop_id = 0; pop_id < c->n_pops; pop_id++) {
      if (POP::Get_Race(c->pop[pop_id]) >= 8) { continue; }
      if (POP::Get_Prof(c->pop[pop_id]) != POP::PROF_WORKER) { continue; }
      Score_Scientist_Candidate_(candidates + nb_candidates, cid, pop_id);
      nb_candidates++;
    }
    qsort(candidates, nb_candidates, sizeof(*candidates), cmp_pop_with_score_dec);
        
    for (int candidate_id = 0; candidate_id < nb_candidates; candidate_id++) {
      struct pop_with_score * cand = candidates + candidate_id;
      POP::Set_Prof(c->pop + cand->pop_id, POP::PROF_SCIENTIST);
      COLCALC::Colony_Industry_Production(c, nullptr);
      if (c->production[ECON_INDUSTRY] < c->maintenance[ECON_INDUSTRY] + preserve_prod
          || (had_pollution && c->pollution == 0)) {
        POP::Set_Prof(c->pop + cand->pop_id, POP::PROF_WORKER);
        COLCALC::Colony_Industry_Production(c, nullptr);
        break;
      }
    }
  }

  static void __cdecl Move_Some_Colonists_To_Research_(struct AITK::aidata * aidata) { 
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }

      // Special case: rush to get early tech (freighters, lab, factories, cships)
      if (p->tech_fields[TECHDATA::technology_applications[TECH_APP_AUTOMATED_FACTORIES].tech_field_id] != 3
          || p->tech_fields[TECHDATA::technology_applications[TECH_APP_RESEARCH_LABORATORY].tech_field_id] != 3
          || p->tech_fields[TECHDATA::technology_applications[TECH_APP_STANDARD_FUEL_CELLS].tech_field_id] != 3
          || (aidata->combat_command_points[pid] >= 6 &&
              (p->tech_fields[TECHDATA::technology_applications[TECH_APP_BATTLE_SCANNER].tech_field_id] != 3
               || p->tech_fields[TECHDATA::technology_applications[TECH_APP_COLONY_SHIP].tech_field_id] != 3))) {
        for (int i = 0; i < aidata->nb_colonies_by_player[pid]; i++) {
          int cid = aidata->colonies_by_player[pid][i];
          if (EVENTS::Event_Check_Space_Anomaly(cid)) { continue; }
          struct s_colony * c = MOX::colony+cid;
          int high_prod = (c->production[ECON_INDUSTRY] - c->maintenance[ECON_INDUSTRY]) * 8 >= COLCALC::Colony_Product_Cost(c, BUILDING_COLONY_BASE);
          if (high_prod) { aidata->high_prod_start[pid] = 1; }
          if (c->producing[0] == COLONY_PRODUCTION_AI_STOCKPILE
              || COLBLDG::Colony_Production_Is_Queued_Ship(c->producing[0])
              || ((c->producing[0] == COLONY_PRODUCTION_FREIGHTERS || c->producing[0] == BUILDING_COLONY_BASE)
                  && (!high_prod || (p->tech_fields[TECHDATA::technology_applications[TECH_APP_FREIGHTERS].tech_field_id] != 3
                                     && !p->traits[TRAIT_EATS_MINERALS])))) {
            Make_Research_Colony_(cid, 0);
            if (c->producing[0] == BUILDING_COLONY_BASE || COLBLDG::Colony_Production_Is_Queued_Ship(c->producing[0])) {
              COLBLDG::Delete_From_Colony_Queue(cid, 0);
              if (c->producing[0] == COLONY_PRODUCTION_NONE) {
                c->producing[0] = COLONY_PRODUCTION_AI_STOCKPILE;
              }
            }
          }
        }
        continue;
      }

      // Move some dudes to research in the general case
      for (int i = 0; i < aidata->nb_colonies_by_player[pid]; i++) {
        int cid = aidata->colonies_by_player[pid][i];
        if (EVENTS::Event_Check_Space_Anomaly(cid)) { continue; }
        struct s_colony * c = MOX::colony+cid;
        if (c->producing[0] == COLONY_PRODUCTION_AI_STOCKPILE || EVENTS::Event_Check_Plague(cid)) {
          Make_Research_Colony_(cid, 0);
        } else {
          int cost = COLCALC::Colony_Producing_Product_Cost(c);
          if (!COLBLDG::Colony_Production_Is_Building(c->producing[0])
              && cost > 0
              && c->production[ECON_INDUSTRY] - c->maintenance[ECON_INDUSTRY]
              + c->production_spent + c->bought_outright > cost) {
            Make_Research_Colony_(cid, cost - (c->production_spent + c->bought_outright));
          }
        }
      }
      // TODO: stop big projects on colonies whose production has dropped
    }
    
    COLCALC::All_Colony_Calculations();
  }
        
  void __cdecl Buy_Products_(struct AITK::aidata * aidata) {
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      
      qsort(aidata->unblockaded_colonies_by_player[pid], aidata->nb_unblockaded_colonies_by_player[pid], sizeof(*aidata->unblockaded_colonies_by_player[pid]), cmp_colony_industry_inc);

      for (int phase = 0; phase < 2; phase++) {
        for (int i = 0; i < aidata->nb_colonies_by_player[pid]; i++) {
          int buffer = 50;
          if (phase > 0) { buffer = 200; }
          if (p->surplus_bc < 0) {
            buffer -= p->surplus_bc*10;
          }
          if (p->bc + p->surplus_bc < buffer) { break; }
          
          int cid = aidata->colonies_by_player[pid][i];
          if (EVENTS::Event_Check_Space_Anomaly(cid)) { continue; }
          struct s_colony * c = MOX::colony+cid;
          if (COLCALC::Colony_Is_Blockaded(c)) { continue; }
          if (c->bought_outright) { continue; }
          int prod = c->producing[0];
          if (prod == COLONY_PRODUCTION_NONE || prod == COLONY_PRODUCTION_AI_STOCKPILE
              || prod == COLONY_PRODUCTION_TRADE_GOODS || prod == COLONY_PRODUCTION_HOUSING) {
            continue;
          }
          int prod_cost = COLCALC::Colony_Producing_Product_Cost(c);
          if (phase > 0 || Is_Priority_Buy_(c->producing[0])
              || c->production_spent * 2 >= prod_cost) {
            int bc_cost = COLCALC::Colony_Cost_To_Buy_Producing_Product(c);
            if (bc_cost <= p->bc && p->bc - bc_cost + p->surplus_bc >= buffer) {
              COLCALC::Colony_Buy_Outright(cid);
              COLCALC::All_Colony_Calculations();
              aidata->buy_capacity[pid] -= bc_cost/2;
            }
          }
        }
      }
    }
  }

  void __cdecl System_Buildings_(int pid, int stid, int nb_colonies, int * colonies) {
    struct s_star_data * st = &MOX::star[stid];
    struct AITK::int_pair colonies_with_score[nb_colonies];
    
    for (int i = 0; i < nb_colonies; i++) {
      colonies_with_score[i].y = colonies[i];
      colonies_with_score[i].x = AITK::Planet_Score_(&MOX::planet[MOX::colony[colonies[i]].planet], pid, 1);
    }
    qsort(colonies_with_score, nb_colonies, sizeof(*colonies_with_score), AITK::cmp_int_pair_dec);
    
    int nb_planets_available = 0;
    int nb_uninhabitable_available = 0;
    int nb_cbases = 0;
    for (int orbit = 0; orbit < 5; orbit++) {
      int plid = st->planet_index[orbit];
      struct s_planet_data * pl;
      if (plid >= 0) { pl = &MOX::planet[plid]; }
      else { continue; }
      int enemy = 0;
      int colony = 0;
      if (pl->colony_index >= 0) {
        struct s_colony * c = &MOX::colony[pl->colony_index];
        if (c->owner != pid) { enemy = 1; }
        if (!c->outpost_flag) { colony = 1; }
      }
      if (pl->planet_type == PLANET_TYPE_PLANET && !enemy && !colony) {
        nb_planets_available++;
      }
      if ((pl->planet_type == PLANET_TYPE_ASTEROID || pl->planet_type == PLANET_TYPE_GAS_GIANT)
          && !enemy) {
        nb_uninhabitable_available++;
      }
    }

    for (int i = 0; i < nb_colonies; i++) {
      int cid = colonies_with_score[i].y;
      struct s_colony * c = &MOX::colony[cid];
      if (c->producing[0] == BUILDING_COLONY_BASE) {
        if (nb_planets_available > 0) {
          nb_planets_available--;
          nb_cbases++;
        } else if (!c->bought_outright) {
          c->producing[0] = COLONY_PRODUCTION_AI_STOCKPILE;
          nb_cbases++;
        }
      } else if (c->producing[0] == BUILDING_ARTIFICIAL_PLANET) {
        if (nb_uninhabitable_available > 0) {
          nb_uninhabitable_available--;
        } else if (!c->bought_outright) {
          c->producing[0] = COLONY_PRODUCTION_AI_STOCKPILE;
        }
      }
    }
    
    for (int i = 0; i < nb_colonies && nb_planets_available > 0; i++) {
      int cid = colonies_with_score[i].y;
      struct s_colony * c = &MOX::colony[cid];
      if (c->bought_outright) { continue; }
      if (c->producing[0] != BUILDING_COLONY_BASE
          && (c->producing[0] == COLONY_PRODUCTION_AI_STOCKPILE
              || (COLBLDG::Colony_Production_Is_Building(c->producing[0])
                  && !Is_Priority_Buy_(c->producing[0])
                  && c->producing[0] != BUILDING_CLONING_CENTER
                  && c->producing[0] != BUILDING_RESEARCH_LAB))
          && ((nb_cbases == 0 && (c->n_pops >= 5 || !Colony_Can_Grow(c)))
              || c->production_spent +
              (c->production[ECON_INDUSTRY] - c->maintenance[ECON_INDUSTRY]) * 5
              >= COLCALC::Colony_Product_Cost(c, BUILDING_COLONY_BASE))) {
        c->producing[0] = BUILDING_COLONY_BASE;
        nb_planets_available--;
        nb_cbases++;
      }
    }
  }
  
  static void __cdecl Manage_One_System_(struct AITK::aidata * aidata, int pid, int stid, int nb_colonies, int * colonies) {    
    struct s_star_data * st = &MOX::star[stid];
    struct AITK::int_pair colonies_with_score[nb_colonies];

    if (st->blockaded & (1 << pid)) { return; }
    
    for (int i = 0; i < nb_colonies; i++) {
      int cid = colonies[i];
      struct s_colony * c = &MOX::colony[cid];
      colonies_with_score[i].y = cid;
      if (Is_Kickstart_Build_(c->producing[0])) {
        colonies_with_score[i].x = INT_MAX;
      } else {
        colonies_with_score[i].x = AITK::Planet_Score_(&MOX::planet[MOX::colony[colonies[i]].planet], pid, 1);
        if (colonies_with_score[i].x == INT_MAX) { colonies_with_score[i].x--; }
      }
    }
    qsort(colonies_with_score, nb_colonies, sizeof(*colonies_with_score), AITK::cmp_int_pair_dec);

    int dst_i = 0;
    int src_i = nb_colonies-1;
    int pop_index = 0;
    int nb_growers;
    int src_changed = 1;
    while (src_i > dst_i) {
      s_colony * src = &MOX::colony[colonies_with_score[src_i].y];
      s_colony * dst = &MOX::colony[colonies_with_score[dst_i].y];
      if (src_changed) {
        nb_growers = 0;
        for (int i = 0 ; i < src->n_pops ; i++) {
          if (POP::Get_Race(src->pop[i]) < 8) { nb_growers++; }
        }
        src_changed = 0;
      }
      while (pop_index < src->n_pops) {
        uint32_t pop = src->pop[pop_index];
        if (POP::Get_Race(pop) >= 8
            || (POP::Get_Prof(pop) == POP::PROF_FARMER &&
                COLCALC::Colony_Pop_Prod_Produced(src, &pop, ECON_FOOD) >= COLCALC::Colony_Pop_Prod_Produced(dst, &pop, ECON_FOOD))
            || POP::Is_Conquered(pop)) {
          pop_index++;
        } else if (colonies_with_score[dst_i].x == INT_MAX &&
                   dst->n_pops > 5) {
          dst_i++;
          break;
        } else {
          struct s_settler_settle_result settle_result;
          SETTLER::Pop_Tries_To_Settle(src, pop_index, dst, &settle_result);
          if (nb_growers > 1 && settle_result.can_settle
              && !settle_result.no_room_at_destination
              && !settle_result.dest_blockaded
              && !settle_result.dest_space_anomaly) {
            SETTLER::Settle_Pop(src, pop_index, dst);
            nb_growers--;
          } else if (nb_growers <= 1
                     || settle_result.source_blockaded
                     || settle_result.source_space_anomaly
                     || settle_result.source_plague
                     || settle_result.only_one_pop_left) {
            src_changed = 1;
            break;
          } else {
            dst_i++;
            break;
          }
        }
      }
      if (pop_index >= src->n_pops) {
        src_changed = 1;
      }
      if (src_changed) {
        src_i--;
        pop_index = 0;
      }
    }

    for (int i = 0; i < nb_colonies; i++) {
      int cid = colonies_with_score[i].y;
      struct s_colony * c = &MOX::colony[cid];
      if (c->bought_outright) { continue; }
      if (c->n_pops < 5 && !COLBLDG::Colony_Production_Is_Queued_Ship(c->producing[0])
          && !Is_Droid_(c->producing[0])
          && (i > 0 || !Is_Kickstart_Build_(c->producing[0]))
          && Colony_Can_Grow(c)) {
        c->producing[0] = COLONY_PRODUCTION_HOUSING;
      } else if (c->producing[0] == COLONY_PRODUCTION_HOUSING && !Colony_Can_Grow(c)) {
        c->producing[0] = COLONY_PRODUCTION_AI_STOCKPILE;
      }

      if (Is_Kickstart_Build_(c->producing[0])) {
        aidata->no_shipping_at_star[pid][stid] = 1;
      }
    }
  }
  
  void __cdecl Manage_Systems_(struct AITK::aidata * aidata) {
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }

      
      for (int stid = 0; stid < MOX::NUM_STARS; stid++) {
        struct s_star_data * st = &MOX::star[stid];
        if (EVENTS::Event_Check_Space_Anomaly_At_Star(stid)) { continue; }
        if (!(st->has_colony & (1<<pid))) { continue; }
        
        int nb_my_colonies = 0;
        int my_colonies[5];

        for (int orbit = 0; orbit < 5; orbit++) {
          int plid = st->planet_index[orbit];
          struct s_planet_data * pl;
          if (plid >= 0) { pl = &MOX::planet[plid]; }
          else { continue; }
          if (pl->planet_type == PLANET_TYPE_PLANET
              && pl->colony_index >= 0) {
            struct s_colony * c = &MOX::colony[pl->colony_index];
            if (!c->outpost_flag && c->owner == pid) {
              my_colonies[nb_my_colonies] = pl->colony_index;
              nb_my_colonies++;
            }
          }
        }

        System_Buildings_(pid, stid, nb_my_colonies, my_colonies);
        if (nb_my_colonies > 1) {
          Manage_One_System_(aidata, pid, stid, nb_my_colonies, my_colonies);
        }
      }
    }

    COLCALC::All_Colony_Calculations();
  }

  int __cdecl Can_Build_Unrealized_Product_(struct s_player * p, struct s_colony * c, int b) {
    if (b == COLONY_PRODUCTION_COLONY_SHIP
        || b == COLONY_PRODUCTION_OUTPOST_SHIP
        || b == COLONY_PRODUCTION_TRANSPORT_SHIP) {
      if (p->ship_range == 0 || p->ship_speed == 0) {
        return 0;
      } else {
        switch (b) {
        case COLONY_PRODUCTION_COLONY_SHIP:
          return (p->tech_applications[TECH_APP_COLONY_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED);
        case COLONY_PRODUCTION_OUTPOST_SHIP:
          return (p->tech_applications[TECH_APP_OUTPOST_SHIP] == TECH_RESEARCH_STATUS_RESEARCHED);
        case COLONY_PRODUCTION_TRANSPORT_SHIP:
          return (p->tech_applications[TECH_APP_TRANSPORT] == TECH_RESEARCH_STATUS_RESEARCHED
                  && c->buildings[BUILDING_MARINE_BARRACKS]);
        }
        return 0;
      }
    } else {
      return COLCALC::Colony_Can_Build_Product(c, b) == 1;
    }
  }
  
  void __cdecl Empire_Building_Small_(struct AITK::aidata * aidata) {
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      
      int transports_wanted = aidata->combat_command_points[pid]/3;
      if (transports_wanted < 4) { transports_wanted = 0; }
      if (p->traits[TRAIT_TELEPATHIC]) { transports_wanted = 0; }
      if (aidata->nb_threatens[pid] == 0) { transports_wanted = 0; }
      int new_transports_needed = std::max(0, transports_wanted - aidata->nb_transports[pid]);
      int max_queued_transports = (aidata->nb_colonies_by_player[pid]+5)/6;

      int spies_wanted = std::min(63, (aidata->population[pid]-30)/5);
      if (aidata->nb_contacts[pid] == 0) { spies_wanted = 0; }
      int new_spies_needed = std::max(0, spies_wanted - aidata->nb_spies[pid]);
      int max_queued_spies = (aidata->nb_colonies_by_player[pid]+9)/10;

      // TODO: fix this
      int new_freighters_needed = p->surplus_freighters >= std::min(10, 5*(aidata->nb_colonies_by_player[pid]-1)) ? 0 : 1;
      
      struct AITK::int_pair empire_needs[] = {
        { .x = COLONY_PRODUCTION_TRANSPORT_SHIP,
          .y = std::max(0, std::min(new_transports_needed, max_queued_transports) - aidata->nb_queued_transports[pid]) },
        { .x = COLONY_PRODUCTION_SPY,
          .y = std::max(0, std::min(new_spies_needed, max_queued_spies) - aidata->nb_queued_spies[pid]) },
        { .x = COLONY_PRODUCTION_FREIGHTERS,
          .y = std::max(0, new_freighters_needed) },
      };
      int nb_empire_needs = sizeof(empire_needs) / sizeof(*empire_needs);

      qsort(aidata->colonies_by_player[pid], aidata->nb_colonies_by_player[pid], sizeof(*aidata->colonies_by_player[pid]), cmp_colony_industry_inc);
      
      uint8_t colony_used[aidata->nb_colonies_by_player[pid]];
      memset(colony_used, 0, aidata->nb_colonies_by_player[pid]);

      for (int need_rank = nb_empire_needs-1; need_rank >= 0; need_rank--) {
        for (int colony_rank = 0; empire_needs[need_rank].y > 0 && colony_rank < aidata->nb_colonies_by_player[pid]; colony_rank++) {
          
          int cid = aidata->colonies_by_player[pid][colony_rank];
          struct s_colony * c = &MOX::colony[cid];
          
          if (!colony_used[colony_rank]
              && !EVENTS::Event_Check_Space_Anomaly(cid)
              && !c->bought_outright
              && (c->producing[0] == COLONY_PRODUCTION_AI_STOCKPILE || c->producing[0] == BUILDING_COLONY_BASE)) {

            int b = empire_needs[need_rank].x;
            
            if (b == COLONY_PRODUCTION_TRANSPORT_SHIP && !c->buildings[BUILDING_MARINE_BARRACKS]) {
              b = BUILDING_MARINE_BARRACKS;
            }
            
            int cost = COLCALC::Colony_Product_Cost(c, b);
            int prod = std::max(c->production[ECON_INDUSTRY] - c->maintenance[ECON_INDUSTRY], 0);
            if (Can_Build_Unrealized_Product_(p, c, b)) {
              int do_build = 0;
              if (c->production_spent + prod >= cost) {
                do_build = 1;
              } else if (!COLCALC::Colony_Is_Blockaded(c)
                         && c->production_spent + aidata->buy_capacity[pid] >= cost
                         && c->production_spent * 2 >= cost) {
                do_build = 1;
                aidata->buy_capacity[pid] -= (cost - c->production_spent);
              }
              if (do_build) {
                empire_needs[need_rank].y--;
                c->producing[0] = b;
                colony_used[colony_rank] = 1;
              }
            }
          }
        }
      }
    }
  }

  void __cdecl Empire_Building_Big_(struct AITK::aidata * aidata) {
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }

      int colony_ships_wanted = std::min(2, aidata->nb_colonizable_planets[pid]);
      if (aidata->strategy[pid] != AITK::STRAT_EXPAND) { colony_ships_wanted = 0; }
      int new_colony_ships_needed = std::max(0, colony_ships_wanted - (aidata->nb_colony_ships[pid] + aidata->nb_queued_colony_ships[pid]));

      int combat_command_points_wanted = (p->command_points + std::max<int>(0, p->surplus_bc-10)/10)*3/4; // TODO: fix this formula
      if (aidata->strategy[pid] == AITK::STRAT_DROIDS) { combat_command_points_wanted = 0; }
      if (aidata->nb_threats[pid] == 0 && aidata->nb_threatens[pid] == 0) { combat_command_points_wanted = std::min(6, combat_command_points_wanted); }
      if (aidata->nb_contacts[pid] == 0 && aidata->nb_stars_by_player[pid] + aidata->nb_colony_ships[pid] < 3) { combat_command_points_wanted = std::min(3, combat_command_points_wanted); }
      int new_combat_command_points_needed = std::max(0, combat_command_points_wanted - (aidata->combat_command_points[pid] + aidata->queued_combat_command_points[pid]));
      
      struct AITK::int_pair empire_needs[] = {
        { .x = COLONY_PRODUCTION_COLONY_SHIP,
          .y = new_colony_ships_needed },
        { .x = COLONY_PRODUCTION_CUSTOM_DESIGN,
          .y = new_combat_command_points_needed },
      };
      int nb_empire_needs = sizeof(empire_needs) / sizeof(*empire_needs);

      qsort(aidata->colonies_by_player[pid], aidata->nb_colonies_by_player[pid], sizeof(*aidata->colonies_by_player[pid]), cmp_colony_industry_inc);
      
      uint8_t colony_used[aidata->nb_colonies_by_player[pid]];
      memset(colony_used, 0, aidata->nb_colonies_by_player[pid]);

      int threshold = std::max(0, (aidata->nb_colonies_by_player[pid]/2)-1);
      if (p->tech_fields[TECHDATA::technology_applications[TECH_APP_PLANETARY_SUPERCOMPUTER].tech_field_id] != 3) {
        threshold = std::max(0, aidata->nb_colonies_by_player[pid]-2);
      }
      for (int need_rank = nb_empire_needs-1; need_rank >= 0; need_rank--) {
        for (int colony_rank = aidata->nb_colonies_by_player[pid]-1; colony_rank >= threshold && empire_needs[need_rank].y > 0; colony_rank--) {
          
          int cid = aidata->colonies_by_player[pid][colony_rank];
          struct s_colony * c = &MOX::colony[cid];
        
          if (!colony_used[colony_rank]
              && !EVENTS::Event_Check_Space_Anomaly(cid)
              && !c->bought_outright
              && (c->producing[0] == COLONY_PRODUCTION_AI_STOCKPILE)){ 

            int b = empire_needs[need_rank].x;
            int prod = std::max(c->production[ECON_INDUSTRY] - c->maintenance[ECON_INDUSTRY], 0);

            int nb = 1;
            if (b == COLONY_PRODUCTION_CUSTOM_DESIGN) {
              int cost_small = COLCALC::Colony_Product_Cost(c, COLBLDG::Colony_Production_From_Ship_Design_Index(aidata->military_ship_small[pid]));
              if (aidata->military_ship_big[pid] == HULL_DOOM_STAR) {
                memcpy(&p->ship_designs[HULL_DOOM_STAR], &(aidata->doom_star_design[pid]), sizeof(struct s_ship_design));
              }
              int cost_big = COLCALC::Colony_Product_Cost(c, COLBLDG::Colony_Production_From_Ship_Design_Index(aidata->military_ship_big[pid]));
              if (c->production_spent + 10*prod >= cost_big
                  && aidata->game_turn >= (MOX::settings.starting_civilization_level == STARTING_CIVILIZATION_LEVEL_PRE_WARP ? 60 : 50)) {
                nb = aidata->military_ship_big[pid]+1;
                b = COLBLDG::Colony_Production_From_Ship_Design_Index(aidata->military_ship_big[pid]);
              } else if (c->production_spent + 10*prod >= cost_small) {
                nb = aidata->military_ship_small[pid]+1;
                b = COLBLDG::Colony_Production_From_Ship_Design_Index(aidata->military_ship_small[pid]);
              } else {
                continue;
              }
              if (COLCALC::Colony_Can_Build_Large_Ships(c)) {
                if (p->tech_applications[TECH_APP_SPACE_ACADEMY] == TECH_RESEARCH_STATUS_RESEARCHED && !c->buildings[BUILDING_SPACE_ACADEMY]) {
                  b = BUILDING_SPACE_ACADEMY;
                }
              } else {
                b = BUILDING_STAR_BASE;
              }
            } else {
              int cost = COLCALC::Colony_Product_Cost(c, b);
              if (!(c->production_spent + 10*prod >= cost)) { continue; }
            }
            
            if (Can_Build_Unrealized_Product_(p, c, b)) {
              empire_needs[need_rank].y -= nb;
              c->producing[0] = b;
              colony_used[colony_rank] = 1;
            }
          }

          else if (empire_needs[need_rank].x == COLONY_PRODUCTION_CUSTOM_DESIGN
                   && !colony_used[colony_rank]
                   && !EVENTS::Event_Check_Space_Anomaly(cid)
                   && (c->producing[0] == BUILDING_SPACE_ACADEMY || c->producing[0] == BUILDING_STAR_BASE)) {
            int cost_small = COLCALC::Colony_Product_Cost(c, COLBLDG::Colony_Production_From_Ship_Design_Index(aidata->military_ship_small[pid]));
            int cost_big = COLCALC::Colony_Product_Cost(c, COLBLDG::Colony_Production_From_Ship_Design_Index(aidata->military_ship_big[pid]));
            int prod = std::max(c->production[ECON_INDUSTRY] - c->maintenance[ECON_INDUSTRY], 0);
            if (c->production_spent + 10*prod >= cost_big
                && aidata->game_turn >= (MOX::settings.starting_civilization_level == STARTING_CIVILIZATION_LEVEL_PRE_WARP ? 50 : 40)) {
              empire_needs[need_rank].y -= (aidata->military_ship_big[pid]+1);
            } else if (c->production_spent + 10*prod >= cost_small) {
              empire_needs[need_rank].y -= (aidata->military_ship_small[pid]+1);
            }
          }
        }
      }
    }
  }

  void __cdecl Opportunistic_Local_Build_(struct AITK::aidata * aidata) { 
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      int products[] = {
        BUILDING_STAR_BASE, BUILDING_BATTLESTATION, BUILDING_STAR_FORTRESS,
        BUILDING_FLUX_SHIELD, BUILDING_BARRIER_SHIELD,
        BUILDING_WARP_INTERDICTOR, BUILDING_STELLAR_CONVERTER, BUILDING_ARTEMIS_SYSTEM_NET, BUILDING_ARMOR_BARRACKS
      };
      int nb_products = sizeof(products) / sizeof(*products);
      
      qsort(aidata->colonies_by_player[pid], aidata->nb_colonies_by_player[pid], sizeof(*aidata->colonies_by_player[pid]), cmp_colony_industry_inc);

      for (int colony_rank = 0; colony_rank < aidata->nb_colonies_by_player[pid]; colony_rank++) {
        int cid = aidata->colonies_by_player[pid][colony_rank];
        struct s_colony * c = &MOX::colony[cid];
        if (EVENTS::Event_Check_Space_Anomaly(cid)) { continue; }
        if (c->producing[0] != COLONY_PRODUCTION_AI_STOCKPILE) { continue; }
        if (c->bought_outright) { continue; }

        for (int product_rank = 0; product_rank < nb_products; product_rank++) {
          int b = products[product_rank];
          if (COLCALC::Colony_Can_Build_Product(c, b) != 1) { continue; }
            
          int cost = COLCALC::Colony_Product_Cost(c, b);
          if (c->production_spent + std::max(c->production[ECON_INDUSTRY] - c->maintenance[ECON_INDUSTRY], 0) >= cost) {
            c->producing[0] = b;
            break;
          } else if (!COLCALC::Colony_Is_Blockaded(c)
                     && c->production_spent + aidata->buy_capacity[pid] >= cost
                     && c->production_spent * 2 >= cost) {
            c->producing[0] = b;
            aidata->buy_capacity[pid] -= (cost - c->production_spent);
            break;
          }
        }
      }
    }
  }

  void __cdecl Optimize_Farming_(struct AITK::aidata * aidata) {
    Move_All_Colonists_To_industry_();
    Unblockaded_Farming_(aidata);
    Blockaded_Farming_(aidata);
  }

  void __cdecl Realize_Products_(struct AITK::aidata * aidata) {
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      for (int i = 0; i < aidata->nb_colonies_by_player[pid]; i++) {
        int cid = aidata->colonies_by_player[pid][i];
        struct s_colony * c = &MOX::colony[cid];
        if (c->producing[0] == COLONY_PRODUCTION_COLONY_SHIP
            || c->producing[0] == COLONY_PRODUCTION_OUTPOST_SHIP
            || c->producing[0] == COLONY_PRODUCTION_TRANSPORT_SHIP) {
          s_ship_design design;
          int ship_id;
          switch (c->producing[0]) {
          case COLONY_PRODUCTION_COLONY_SHIP:
            INITSHIP::Load_Colony_Ship_Design(pid, &design);
            break;
          case COLONY_PRODUCTION_OUTPOST_SHIP:
            INITSHIP::Load_Outpost_Ship_Design(pid, &design);
            break;
          case COLONY_PRODUCTION_TRANSPORT_SHIP:
            INITSHIP::Load_Transport_Ship_Design(pid, &design);
            break;
          }
          ship_id = COLBLDG::Create_Ship_From_Design(pid, &design, cid);
          if (ship_id >= 0) {
            MOX::ship[ship_id].status = 6;
            c->producing[0] = COLBLDG::Colony_Production_From_Ship_Index(ship_id);
          } else {
            c->producing[0] = COLONY_PRODUCTION_AI_STOCKPILE;
          }
        } else if (COLBLDG::Colony_Production_Is_Ship_Design(c->producing[0])) {
          int design_index = COLBLDG::Colony_Production_Ship_Design_Index(c->producing[0]);
          int ship_id;
          if (design_index == HULL_DOOM_STAR) {
            ship_id = COLBLDG::Create_Ship_From_Design(pid, &(aidata->doom_star_design[pid]), cid);
          } else {
            ship_id = COLBLDG::Create_Ship(pid, design_index, cid);
          }
          if (ship_id >= 0) {
            MOX::ship[ship_id].status = 6;
            c->producing[0] = COLBLDG::Colony_Production_From_Ship_Index(ship_id);
          } else {
            c->producing[0] = COLONY_PRODUCTION_AI_STOCKPILE;
          }
        }
      }
    }
  }

  static int __cdecl Colony_Has_More_Than_One_Grower_(struct s_colony * c) {
    int nb_growers = 0;
    for (int i = 0 ; i < c->n_pops ; i++) {
      if (POP::Get_Race(c->pop[i]) < 8) { nb_growers++; }
      if (nb_growers > 1) { return 1; }
    }
    return 0;
  }
        
  static void __cdecl Ship_Colonists_(struct AITK::aidata * aidata) {
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }

      struct AITK::int_pair colonies_with_score[aidata->nb_unblockaded_colonies_by_player[pid]];
      int nb_colonies = 0;
      for (int i = 0; i < aidata->nb_unblockaded_colonies_by_player[pid]; i++) { 
        int cid = aidata->unblockaded_colonies_by_player[pid][i];
        struct s_colony * c = &MOX::colony[cid];
        if (EVENTS::Event_Check_Space_Anomaly(cid)) { continue; }
        int stid = MOX::planet[c->planet].star_index;
        if (aidata->no_shipping_at_star[pid][stid]) { continue;  }
        int score = AITK::Planet_Score_(&MOX::planet[c->planet], pid, 1);
        colonies_with_score[nb_colonies].y = cid;
        colonies_with_score[nb_colonies].x = score;
        nb_colonies++;
      }
      if (nb_colonies == 0) { continue; }
      qsort(colonies_with_score, nb_colonies, sizeof(*colonies_with_score), AITK::cmp_int_pair_dec);
      int score_cutoff = colonies_with_score[nb_colonies / 4].x; // The top quartile becomes dest even if housing

      int source_pop_by_race[8];
      for (int i = 0; i < 8; i++) {
        source_pop_by_race[i] = 0;
      }
      
      struct AITK::int_pair dest_colonies_with_score[aidata->nb_unblockaded_colonies_by_player[pid]];
      struct AITK::int_pair source_colonies_with_score[aidata->nb_unblockaded_colonies_by_player[pid]];
      int nb_dest_colonies = 0;
      int nb_source_colonies = 0;
      for (int i = 0; i < nb_colonies; i++) {
        int cid = colonies_with_score[i].y;
        struct s_colony * c = &MOX::colony[cid];
        int score = colonies_with_score[i].x;
        if (c->producing[0] != COLONY_PRODUCTION_HOUSING || score >= score_cutoff) {
          dest_colonies_with_score[nb_dest_colonies].y = cid;
          dest_colonies_with_score[nb_dest_colonies].x = score;
          nb_dest_colonies++;
        } else if (!EVENTS::Event_Check_Plague(cid)
                   && Colony_Has_More_Than_One_Grower_(c)) {
          source_colonies_with_score[nb_source_colonies].y = cid;
          source_colonies_with_score[nb_source_colonies].x = score;
          nb_source_colonies++;
          for (int pop_index = 0 ; pop_index < c->n_pops ; pop_index++) {
            int r = POP::Get_Race(c->pop[pop_index]);
            if (r < 8) { source_pop_by_race[r]++; }
          }
        }
      }
      qsort(dest_colonies_with_score, nb_dest_colonies, sizeof(*dest_colonies_with_score), AITK::cmp_int_pair_dec);
      qsort(source_colonies_with_score, nb_source_colonies, sizeof(*source_colonies_with_score), AITK::cmp_int_pair_inc);

      // While all dest colonies are full, move a colony from source to dest
      int nb_dest_colonies_not_full = 0;
      for (int dest_rank = 0; dest_rank < nb_dest_colonies; dest_rank++) {
        int dest_id = dest_colonies_with_score[dest_rank].y;
        int dest_score = dest_colonies_with_score[dest_rank].x;
        struct s_colony * dest = &MOX::colony[dest_id];
        for (int r = 0; r < 8; r++) {
          if (source_pop_by_race[r] > 0) {
            if (SETTLER::Room_For_Another(dest, r, 1)) {
              nb_dest_colonies_not_full++;
              break;
            }
          }
        }
      }
      while (nb_dest_colonies_not_full == 0 && nb_source_colonies > 0) {
        int cid = source_colonies_with_score[nb_source_colonies-1].y;
        struct s_colony * c = &MOX::colony[cid];
        for (int pop_index = 0 ; pop_index < c->n_pops ; pop_index++) {
          int r = POP::Get_Race(c->pop[pop_index]);
          if (r < 8) { source_pop_by_race[r]--; }
        }

        nb_dest_colonies++;
        memcpy(&dest_colonies_with_score[nb_dest_colonies-1],
               &source_colonies_with_score[nb_source_colonies-1],
               sizeof(*source_colonies_with_score));
        nb_source_colonies--;

        for (int r = 0; r < 8; r++) {
          if (source_pop_by_race[r] > 0) {
            if (SETTLER::Room_For_Another(c, r, 1)) {
              nb_dest_colonies_not_full++;
              break;
            }
          }
        }
      }

      int ship_capacity = p->surplus_freighters / 5;

      // DEBUG
      printf("\n%s (%d):\n", p->race_name, ship_capacity);
      printf("Dest:\n");
      for (int dest_rank = 0; dest_rank < nb_dest_colonies; dest_rank++) {
        int dest_id = dest_colonies_with_score[dest_rank].y;
        int dest_score = dest_colonies_with_score[dest_rank].x;
        struct s_colony * dest = &MOX::colony[dest_id];
        printf("  %s (%d)\n", HACCESS::Get_Planet_Name_No_Prime(dest->planet), dest_score);
      }
      printf("Source:\n");
      for (int src_rank = 0; src_rank < nb_source_colonies; src_rank++) {
        int src_id = source_colonies_with_score[src_rank].y;
        int src_score = source_colonies_with_score[src_rank].x;
        struct s_colony * src = &MOX::colony[src_id];
        printf("  %s (%d)\n", HACCESS::Get_Planet_Name_No_Prime(src->planet), src_score);
      }
        
      for (int dest_rank = 0; dest_rank < nb_dest_colonies && ship_capacity > 0 && nb_source_colonies > 0; dest_rank++) {
        int dest_id = dest_colonies_with_score[dest_rank].y;
        int dest_score = dest_colonies_with_score[dest_rank].x;
        struct s_colony * dest = &MOX::colony[dest_id];
        int dest_star = MOX::planet[dest->planet].star_index;
        int has_room = 0;
        for (int r = 0; r < 8; r++) {
          if (source_pop_by_race[r] > 0 && SETTLER::Room_For_Another(dest, r, 1)) {
            has_room = 1;
            break;
          }
        }
        if (!has_room) { continue; }
        int best_source_rank = -1;
        int best_eta;
        int best_pop_index;
        for (int source_rank = 0; source_rank < nb_source_colonies; source_rank++) {
          int source_id = source_colonies_with_score[source_rank].y;
          int source_score = source_colonies_with_score[source_rank].x;
          struct s_colony * source = &MOX::colony[source_id];
          int source_star = MOX::planet[source->planet].star_index;
          if (source_score >= dest_score) { continue; }
          if (source_star == dest_star) { continue; }
          if (best_source_rank >= 0 && SETTLER::Settler_ETA(p, source_star, dest_star) >= best_eta) {
            continue;
          }
          // TODO: weigh score diff vs eta
          for (int pop_index = 0; pop_index < source->n_pops; pop_index++) {
            int pop = source->pop[pop_index];
            if (POP::Get_Race(pop) >= 8) { continue; }
            s_settler_settle_result settle_result;
            SETTLER::Pop_Tries_To_Settle(source, pop_index, dest, &settle_result);
            if (settle_result.can_settle && !settle_result.no_room_at_destination
                && !settle_result.dest_blockaded && !settle_result.dest_space_anomaly) {
              best_source_rank = source_rank;
              best_eta = settle_result.eta_turns;
              best_pop_index = pop_index;
              break;
            }
          }
        }
        if (best_source_rank >= 0) {
          int source_id = source_colonies_with_score[best_source_rank].y;
          struct s_colony * source = &MOX::colony[source_id];
          // DEBUG
          /*printf("%s %s[%d]->%s[%d] eta %d diff %d\n",
                 p->race_name,
                 MOX::star[MOX::planet[source->planet].star_index].name, source_colonies_with_score[best_source_rank].x,
                 MOX::star[dest_star].name, dest_score,
                 best_eta, dest_score - source_colonies_with_score[best_source_rank].x);*/
          source_pop_by_race[POP::Get_Race(source->pop[best_pop_index])]--;
          SETTLER::Settle_Pop(source, best_pop_index, dest);
          printf("%s -> ", HACCESS::Get_Planet_Name_No_Prime(source->planet));
          printf("%s\n", HACCESS::Get_Planet_Name_No_Prime(dest->planet)); // DEBUG
          ship_capacity--;
          if (!Colony_Has_More_Than_One_Grower_(source)) {
            for (int pop_index = 0 ; pop_index < source->n_pops ; pop_index++) {
              int r = POP::Get_Race(source->pop[pop_index]);
              if (r < 8) { source_pop_by_race[r]--; }
            }
            memcpy(&source_colonies_with_score[best_source_rank],
                   &source_colonies_with_score[nb_source_colonies-1],
                   sizeof(*source_colonies_with_score));
            nb_source_colonies--;
          }
          dest_rank--;
        }
      }
    }
  }

  void __cdecl Reset_Tax_Rates_(struct AITK::aidata * aidata) {
    int changed = 0;
    for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
      if (p->tax_rate != 0) {
        changed = 1;
        p->tax_rate = 0;
      }
    }
    if (changed) {
      COLCALC::All_Colony_Calculations();
    }
  }
  void __cdecl Set_Tax_Rates_(struct AITK::aidata * aidata) {
    int last_tax_rate_tried = 0;
    while (last_tax_rate_tried < 50) {
      int all_ok = 1;
      for (int pid = 0; pid < MOX::NUM_PLAYERS; pid++) {
        struct s_player * p = MOX::player + pid;
        if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }
        if (p->surplus_bc + p->bc < 0) {
          all_ok = 0;
          p->tax_rate = last_tax_rate_tried+10;
        }
      }
      if (all_ok) {
        break;
      } else {
        COLCALC::All_Colony_Calculations();
      }
      last_tax_rate_tried += 10;
    }      
  }

  void Update_Queued_Ships(struct AITK::aidata * aidata) {
    for (int cid = 0; cid < MOX::NUM_COLONIES; cid++) {
      struct s_colony * c = MOX::colony+cid;
      if (c->owner == -1) { continue; }
      if (c->outpost_flag) { continue; }
      if (EVENTS::Event_Check_Space_Anomaly(cid)) { continue; }
      int pid = c->owner;
      struct s_player * p = MOX::player + pid;
      if (p->eliminated || p->objectives == PLAYER_OBJECTIVE_HUMAN) { continue; }

      int product = c->producing[0];
      if (COLBLDG::Colony_Production_Is_Queued_Ship(product)) {
        int16_t shid = COLBLDG::Colony_Production_Ship_Index(product);
        s_ship_data * sh = &MOX::ship[shid];
        if (sh->d.ship_type == SHIP_TYPE_COMBAT) {
          s_ship_design * new_design;
          if (sh->d.size < SHIP_SIZE_DOOM_STAR) {
            new_design = &p->ship_designs[sh->d.size];
          } else {
            new_design = &aidata->doom_star_design[pid];
          }
          memcpy(&sh->d, new_design, sizeof(sh->d));
        }
      }
    }
  }
  
  void __cdecl Manage_Colonies_(struct AITK::aidata * aidata) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "aitk: Manage_Colonies_ start");
    
    Reset_Tax_Rates_(aidata);
      
    Optimize_Farming_(aidata);
    
    Choose_Buildings_(aidata);
    Manage_Systems_(aidata);
    Ship_Colonists_(aidata);
    Optimize_Farming_(aidata); // Rebalance farming after population movements
    Buy_Products_(aidata);
    
    Empire_Building_Big_(aidata);
    Empire_Building_Small_(aidata);
    Opportunistic_Local_Build_(aidata);
    Realize_Products_(aidata);
    Buy_Products_(aidata);
    Update_Queued_Ships(aidata);
    
    Move_Some_Colonists_To_Research_(aidata);
    
    Set_Tax_Rates_(aidata);
    
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "aitk: Manage_Colonies_ end");
  }
} // namsepace AITK_COLONY
