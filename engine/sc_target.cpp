// ==========================================================================
// Dedmonwakeen's Raid DPS/TPS Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "simulationcraft.h"

// ==========================================================================
// Target
// ==========================================================================

// target_t::target_t =======================================================

target_t::target_t( sim_t* s, const std::string& n ) :
    sim( s ), name_str( n ), next( 0 ), race( RACE_HUMANOID ), level( -1 ),
    initial_armor( -1 ), armor( 0 ), block_value( 100 ),
    attack_speed( 2.0 ), attack_damage( 2000 ), weapon_skill( 0 ),
    fixed_health( 0 ), initial_health( 0 ), current_health( 0 ), fixed_health_percentage( 0 ),
    total_dmg( 0 ), adds_nearby( 0 ), initial_adds_nearby( 0 ), resilience( 0 )
{
  for ( int i=0; i < SCHOOL_MAX; i++ ) spell_resistance[ i ] = 0;
  create_options();
}

// target_t::~target_t ======================================================

target_t::~target_t()
{
}

// target_t::id =============================================================

const char* target_t::id()
{
  if ( id_str.empty() )
  {
    id_str = "0xF1300079AA001884,\"Heroic Training Dummy\",0x10a28";
  }

  return id_str.c_str();
}

// target_t::assess_damage ==================================================

void target_t::assess_damage( double amount,
                              const school_type school,
                              int    dmg_type )
{
  total_dmg += amount;

  if ( current_health > 0 )
  {
    current_health -= amount;

    if ( current_health <= 0 )
    {
      if ( sim -> log ) log_t::output( sim, "%s has died.", name() );
    }
    else if ( sim -> debug ) log_t::output( sim, "Target %s has %.0f remaining health", name(), current_health );
  }
}

// target_t::recalculate_health ==============================================

void target_t::recalculate_health()
{
  if ( sim -> expected_time == 0 ) return;

  if ( fixed_health == 0 )
  {
    current_health = total_dmg;
    initial_health = current_health * ( sim -> expected_time / sim -> current_time );
    fixed_health = initial_health;
  }
  else
  {
    double delta_time = sim -> current_time - sim -> expected_time;
    delta_time /= sim -> current_iteration + 1; // dampening factor

    double factor = 1 - ( delta_time / sim -> expected_time );
    if ( factor > 1.5 ) factor = 1.5;
    if ( factor < 0.5 ) factor = 0.5;

    fixed_health *= factor;
  }

  if ( sim -> debug ) log_t::output( sim, "Target fixed health calculated to be %.0f", fixed_health );
}

// target_t::time_to_die =====================================================

double target_t::time_to_die() SC_CONST
{
  if ( initial_health > 0 )
  {
    return sim -> current_time * current_health / total_dmg;
  }
  else
  {
    return sim -> expected_time - sim -> current_time;
  }
}

// target_t::health_percentage ===============================================

double target_t::health_percentage() SC_CONST
{
  if ( fixed_health_percentage > 0 )
  {
    return fixed_health_percentage;
  }
  else if ( initial_health > 0 ) 
  {
    return 100.0 * current_health / initial_health;
  }
  else
  {
    return 100.0 * ( sim -> expected_time - sim -> current_time ) / sim -> expected_time;
  }
}

// target_t::base_armor ======================================================

double target_t::base_armor() SC_CONST
{
  return armor;
}

// target_t::aura_gain ======================================================

void target_t::aura_gain( const char* aura_name , int aura_id )
{
  if( sim -> log ) log_t::output( sim, "Target %s gains %s", name(), aura_name );
}

// target_t::aura_loss ======================================================

void target_t::aura_loss( const char* aura_name , int aura_id )
{
  if( sim -> log ) log_t::output( sim, "Target %s loses %s", name(), aura_name );
}

// target_t::init ============================================================

void target_t::init()
{
  if ( level < 0 )
  {
    level = sim -> max_player_level + 3;
  }

  if ( initial_armor < 0 ) 
  {
    // TO-DO: Fill in the blanks.
    // For level 80+ at least it seems to pretty much follow a trend line of: armor = 280.26168*level - 12661.51713
    switch ( level )
    {
    case 80: initial_armor = 9729; break; 
    case 81: initial_armor = 10034; break; 
    case 82: initial_armor = 10338; break; 
    case 83: initial_armor = 10643; break; 
    case 84: initial_armor = 10880; break; // Need real value
    case 85: initial_armor = 11161; break; // Need real value
    case 86: initial_armor = 11441; break; // Need real value
    case 87: initial_armor = 11682; break;
    case 88: initial_armor = 11977; break;
    default: if ( level < 80 )
               initial_armor = (int) floor ( ( level / 80.0 ) * 9729 ); // Need a better value here.
             break;
    }
  }

  if( resilience > 0 )
  {
    // TO-DO: Needs work.
    // 1414.5 current capped resilience
    resilience = std::min(resilience, 1414.5);
  }

  if ( weapon_skill == 0 ) weapon_skill = 5.0 * level;

  if ( ! race_str.empty() )
  {
    for ( race = RACE_NONE; race < RACE_MAX; race++ )
      if ( race_str == util_t::race_type_string( race ) )
        break;

    if ( race == RACE_MAX )
    {
      sim -> errorf( "'%s' is not a valid value for 'target_race'\n", race_str.c_str() );
      sim -> cancel();
    }
  }

  // Infinite-Stacking De-Buffs
  debuffs.bleeding     = new debuff_t( this, "bleeding",     -1 );
  debuffs.casting      = new debuff_t( this, "casting",      -1 );
  debuffs.invulnerable = new debuff_t( this, "invulnerable", -1 );
  debuffs.vulnerable   = new debuff_t( this, "vulnerable",   -1 );
}

// target_t::reset ===========================================================

void target_t::reset()
{
  if ( sim -> debug ) log_t::output( sim, "Reseting target %s", name() );
  total_dmg = 0;
  armor = initial_armor;
  current_health = initial_health = fixed_health * ( 1.0 + sim -> vary_combat_length * sim -> iteration_adjust() );
  adds_nearby = initial_adds_nearby;
}

// target_t::combat_begin ====================================================

void target_t::combat_begin()
{
  if ( sim -> overrides.bleeding ) debuffs.bleeding -> override();


}

// target_t::combat_end ======================================================

void target_t::combat_end()
{}

// target_t::debuffs_t::snared ===============================================

bool target_t::debuffs_t::snared()
{
  if ( infected_wounds -> check() ) return true;
  if ( judgements_of_the_just -> check() ) return true;
  if ( slow -> check() ) return true;
  if ( thunder_clap -> check() ) return true;
  return false;
}

// target_t::create_options ====================================================

void target_t::create_options()
{
  option_t target_options[] =
  {
    { "target_name",                    OPT_STRING, &( name_str                          ) },
    { "target_race",                    OPT_STRING, &( race_str                          ) },
    { "target_level",                   OPT_INT,    &( level                             ) },
    { "target_health",                  OPT_FLT,    &( fixed_health                      ) },
    { "target_id",                      OPT_STRING, &( id_str                            ) },
    { "target_adds",                    OPT_INT,    &( initial_adds_nearby               ) },
    { "target_resist_holy",             OPT_INT,    &( spell_resistance[ SCHOOL_HOLY   ] ) },
    { "target_resist_shadow",           OPT_INT,    &( spell_resistance[ SCHOOL_SHADOW ] ) },
    { "target_resist_arcane",           OPT_INT,    &( spell_resistance[ SCHOOL_ARCANE ] ) },
    { "target_resist_frost",            OPT_INT,    &( spell_resistance[ SCHOOL_FROST  ] ) },
    { "target_resist_fire",             OPT_INT,    &( spell_resistance[ SCHOOL_FIRE   ] ) },
    { "target_resist_nature",           OPT_INT,    &( spell_resistance[ SCHOOL_NATURE ] ) },
    { "target_armor",                   OPT_INT,    &( initial_armor                     ) },
    { "target_block",                   OPT_INT,    &( block_value                       ) },
    { "target_attack_speed",            OPT_FLT,    &( attack_speed                      ) },
    { "target_attack_damage",           OPT_FLT,    &( attack_damage                     ) },
    { "target_weapon_skill",            OPT_FLT,    &( weapon_skill                      ) },
    { "target_resilience",              OPT_FLT,    &( resilience                        ) },
    { "target_fixed_health_percentage", OPT_FLT,    &( fixed_health_percentage           ) },
    { NULL, OPT_UNKNOWN, NULL }
  };

  option_t::copy( sim -> options, target_options );
}

// target_t::find =========================================================

target_t* target_t::find( sim_t* sim,
                          const std::string& name_str )
{
  for ( target_t* t = sim -> target_list; t; t = t -> next )
    if ( name_str == t -> name() )
      return t;

  return 0;
}

// target_t::create_expression ================================================

action_expr_t* target_t::create_expression( action_t* action,
                                          const std::string& type )
{
   if ( name_str == "level" )
   {
     struct level_expr_t : public action_expr_t
     {
       target_t* target;
       level_expr_t( action_t* a, target_t* t ) : 
         action_expr_t( a, "target_level", TOK_NUM ), target(t) {}
       virtual int evaluate() { result_num = target -> level; return TOK_NUM; }
     };
     return new level_expr_t( action, this );
   }
   if ( type == "time_to_die" )
   {
     struct target_time_to_die_expr_t : public action_expr_t
     {
       target_t* target;
       target_time_to_die_expr_t( action_t* a, target_t* t ) :
         action_expr_t( a, "target_time_to_die", TOK_NUM ), target(t) {}
       virtual int evaluate() { result_num = target -> time_to_die();  return TOK_NUM; }
     };
     return new target_time_to_die_expr_t( action, this );
   }
   else if ( type == "health_pct" )
   {
     struct target_health_pct_expr_t : public action_expr_t
     {
       target_t* target;
       target_health_pct_expr_t( action_t* a, target_t* t ) :
         action_expr_t( a, "target_health_pct", TOK_NUM ), target(t) {}
       virtual int evaluate() { result_num = target -> health_percentage();  return TOK_NUM; }
     };
     return new target_health_pct_expr_t( action, this );
   }

   else if ( type == "adds" )
   {
     struct target_adds_expr_t : public action_expr_t
     {
       target_t* target;
       target_adds_expr_t( action_t* a, target_t* t ) :
         action_expr_t( a, "target_adds", TOK_NUM ), target(t) {}
       virtual int evaluate() { result_num = target -> adds_nearby;  return TOK_NUM; }
     };
     return new target_adds_expr_t( action, this );
   }

   else if ( type == "adds_never" )
   {
     struct target_adds_never_expr_t : public action_expr_t
     {
       target_t* target;
       target_adds_never_expr_t( action_t* a, target_t* t ) :
         action_expr_t( a, "target_adds_never", TOK_NUM ), target(t)
       {
         bool adds = target -> initial_adds_nearby > 0;
         int num_events = ( int ) a -> sim -> raid_events.size();
         for ( int i=0; i < num_events; i++ )
           if ( a -> sim -> raid_events[ i ] -> name_str == "adds" )
         adds = true;
         result_num = adds ? 0.0 : 1.0;
       }
     };
     return new target_adds_never_expr_t( action, this );
   }


   return 0;
}
