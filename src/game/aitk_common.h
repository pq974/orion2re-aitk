#pragma once

namespace AITK {
  struct int_pair {
    int x;
    int y;
  };

  enum e_strategy {
    STRAT_EXPAND,
    STRAT_DROIDS
  };
  
  struct aidata {
    int game_turn;

    enum e_strategy strategy[8];
    
    int population[8];
    
    int colonies_by_player[8][MAX_COLONIES];
    int nb_colonies_by_player[8];
    
    int unblockaded_colonies_by_player[8][MAX_COLONIES];
    int nb_unblockaded_colonies_by_player[8];
    int blockaded_colonies_by_player[8][MAX_COLONIES];
    int nb_blockaded_colonies_by_player[8];

    int stars_by_player[8][MAX_STARS];
    int nb_stars_by_player[8];

    int buy_capacity[8];

    int no_shipping_at_star[8][MAX_STARS];
    
    //int fleet_strength_by_player[8]; // For now: nb ships >= BB
    //int fleet_target_by_player[8]; // For now: 1 if star date >= 3506.0
    
    //int nb_outpost_ships_needed_by_player[8];
    int nb_outpost_ships[8];
    int nb_queued_outpost_ships[8];
    //int nb_colony_ships_needed_by_player[8];
    int nb_colony_ships[8];
    int nb_queued_colony_ships[8];
    int nb_transports[8];
    int nb_queued_transports[8];
    int combat_command_points[8];
    int queued_combat_command_points[8];

    uint8_t preferred_tech[8];

    s_ai_colonizable_planet colonizable_planets[8][MAX_PLANETS];
    int nb_colonizable_planets[8];

    int military_ship_small[8];
    int military_ship_big[8];
    struct s_ship_design doom_star_design[8];

    int queued_command_points[8];

    int nb_contacts[8];
    int threatens[8][8];
    int nb_threats[8];
    int nb_threatens[8];
    int at_risk_of_blitz[8];

    int high_prod_start[8];
    int nb_spies[8];
    int nb_queued_spies[8];
  };

  int __cdecl Planet_Score_(struct s_planet_data * pl, int player_id, int ignore_location);

  
  int cmp_int_pair_dec(const void * a, const void* b);
  int cmp_int_pair_inc(const void * a, const void* b);

  void __cdecl Init_AI_Data_(struct AITK::aidata * aidata);
  void __cdecl Uninit_AI_Data_(struct AITK::aidata * aidata);
  
}
