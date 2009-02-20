/* $Id$ */
/*
   Copyright (C) 2003 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file pathfind.hpp
 * This module contains various pathfinding functions and utilities.
 */

#ifndef PATHFIND_H_INCLUDED
#define PATHFIND_H_INCLUDED

class gamestatus;
class unit;
class unit_map;

#include "map_location.hpp"
#include "pathutils.hpp"
#include "team.hpp"

#include <map>
#include <list>
#include <set>
#include <vector>
#include <functional>


class xy_pred : public std::unary_function<map_location const&, bool>
{
public:
	virtual bool operator()(map_location const&) = 0;
protected:
	virtual ~xy_pred() {}
};

/** Function which, given a location, will find all tiles within 'radius' of that tile */
void get_tiles_radius(const map_location& a, size_t radius,
					  std::set<map_location>& res);

/** Function which, given a set of locations, will find all tiles within 'radius' of those tiles */
void get_tiles_radius(const gamemap& map, const std::vector<map_location>& locs, size_t radius,
					  std::set<map_location>& res, xy_pred *pred=NULL);

enum VACANT_TILE_TYPE { VACANT_CASTLE, VACANT_ANY };

/**
 * Function which will find a location on the board that is
 * as near to loc as possible, but which is unoccupied by any units.
 * If terrain is not 0, then the location found must be of the given terrain type,
 * and must have a path of that terrain type to loc.
 * If no valid location can be found, it will return a null location.
 */
map_location find_vacant_tile(const gamemap& map,
                                   const unit_map& un,
                                   const map_location& loc,
                                   VACANT_TILE_TYPE vacancy=VACANT_ANY);

/** Function which determines if a given location is in an enemy zone of control. */
bool enemy_zoc(gamemap const &map,
               unit_map const &units,
               std::vector<team> const &teams, map_location const &loc,
               team const &viewing_team, unsigned int side, bool see_all=false);

struct cost_calculator
{
	cost_calculator()
		: max_cost_(0.0)
	{
	}

	virtual double cost(const map_location& src, const map_location& loc, const double so_far) const = 0;
	virtual ~cost_calculator() {}

	// This represents the maximum cost that is allowed for a route.
	// Currently there is only one use: Get the remaining movement of units to calculate the movepoints
	// left at the end of the route.
	virtual int get_max_cost() const { return 0; }

	inline double getNoPathValue() const { return (42424242.0); }

private:
	double max_cost_;
};

/**
 * Object which contains all the possible locations a unit can move to,
 * with associated best routes to those locations.
 */
struct paths
{
	paths() : routes() {}

	// Construct a list of paths for the unit at loc.
	// - force_ignore_zocs: find the path ignoring ZOC entirely,
	//                     if false, will use the unit on the loc's ability
	// - allow_teleport: indicates whether unit teleports between villages
	// - additional_turns: if 0, paths for how far the unit can move this turn will be calculated.
	//                     If 1, paths for how far the unit can move by the end of next turn
	//                     will be calculated, and so forth.
	// viewing_team is usually current team, except for Show Enemy Moves etc.
	paths(gamemap const &map,
	      unit_map const &units,
	      map_location const &loc, std::vector<team> const &teams,
	      bool force_ignore_zocs,bool allow_teleport,
		 const team &viewing_team,int additional_turns = 0,
		 bool see_all = false, bool ignore_units = false);

	/** Structure which holds a single route between one location and another. */
	struct route
	{
		route() : steps(), move_left(0), waypoints() {}
		std::vector<map_location> steps;
		int move_left; // movement unit will have left at end of the route.
		struct waypoint
		{
			waypoint(int turns_number = 0, bool in_zoc = false,
					bool do_capture = false, bool is_invisible = false)
				: turns(turns_number), zoc(in_zoc),
					capture(do_capture), invisible(is_invisible) {}
			int turns;
			bool zoc;
			bool capture;
			bool invisible;
		};
		std::map<map_location, waypoint> waypoints;
	};

	typedef std::map<map_location,route> routes_map;
	routes_map routes;
};

std::ostream& operator << (std::ostream& os, const paths::route& rt);

paths::route a_star_search(map_location const &src, map_location const &dst,
                           double stop_at, cost_calculator const *costCalculator,
                           const size_t parWidth, const size_t parHeight,
                           std::set<map_location> const *teleports = NULL);

/**
 * Function which, given a unit and a route the unit can move on,
 * will return the number of turns it will take the unit to traverse the route.
 * adds "turn waypoints" to rt.turn_waypoints.
 * Note that "end of path" is also added.
 */
int route_turns_to_complete(const unit& u, paths::route& rt, const team &viewing_team,
							const unit_map& units, const std::vector<team>& teams, const gamemap& map);

struct shortest_path_calculator : cost_calculator
{
	shortest_path_calculator(const unit& u, const team& t, const unit_map& units,
                             const std::vector<team>& teams, const gamemap& map,
                             bool ignore_unit = false, bool ignore_defense_ = false);
	virtual double cost(const map_location& src, const map_location& loc, const double so_far) const;

	virtual int get_max_cost() const { return movement_left_; }
private:
	unit const &unit_;
	team const &viewing_team_;
	unit_map const &units_;
	std::vector<team> const &teams_;
	gamemap const &map_;
	int const movement_left_;
	int const total_movement_;
	bool const ignore_unit_;
	bool const ignore_defense_;
};

/**
 * Function which only uses terrain, ignoring shroud, enemies, etc.
 * Required by move_unit_fake if the normal path fails.
 */
struct emergency_path_calculator : cost_calculator
{
	emergency_path_calculator(const unit& u, const gamemap& map);
	virtual double cost(const map_location& src, const map_location& loc, const double so_far) const;
	virtual int get_max_cost() const;

private:
	unit const &unit_;
	gamemap const &map_;
};

/**
 * Function which doesn't take anything into account. Used by
 * move_unit_fake for the last-chance case.
 */
struct dummy_path_calculator : cost_calculator
{
	dummy_path_calculator(const unit& u, const gamemap& map);
	virtual double cost(const map_location& src, const map_location& loc, const double so_far) const;
	virtual int get_max_cost() const;

};

#endif
