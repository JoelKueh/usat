#include "cnf.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

cnf::cnf()
{
	clauses.clear();
	literals.clear();
	state.clear();
	decisions.clear();
	var_cnt = 0;
	clause_cnt = 0;
	max_var = 0;
}

int cnf::init(std::string cnf_path)
{
	std::ifstream ifs;
	std::stringstream ss;
	std::string line;
	std::string trash;
	int32_t literal;
	uint32_t variable;

	// open the cnf file
	ifs.open(cnf_path);
	if (!ifs.is_open()) {
		std::cerr << "Failed to open output stream" << std::endl;
		return -1;
	}

	while (std::getline(ifs, line)) {
		ss = std::stringstream(line);

		if (line[0] == 'c') {
			// skip all comments
			continue;
		} else if (line[0] == 'p') {
			// reserve space when encoutnering the header line
			ss >> trash >> trash >> var_cnt >> clause_cnt;
			clauses.reserve(clause_cnt);
			continue;
		} else if (line[0] == '%') {
			// % is an optional end of file marker
			break;
		}

		// Handle all other clause lines
		clauses.push_back({.start_idx = literals.size(), .end_idx = 0});
		while (ss >> literal) {
			if (literal == 0)
				break;
			variable = literal < 0 ? -literal : literal;
			literals.push_back(literal < 0 ? VAR_NEG(variable) : VAR_POS(variable));
			max_var = max_var < variable ? variable : max_var;
		}
		clauses.back().end_idx = literals.size();
	}

	return 0;
}

int8_t cnf::check_clauses()
{
	int8_t result = SIMPLIFY_SAT;
	bool simpl_done = false;
	bool sat = true;
	int32_t cnt;
	var_t v;
	var_state s;
	uint32_t i, j;
	clause c;

	// check all clauses
	for (i = 0; i < clauses.size(); i++) {
		c = clauses[i];
		cnt = clause_count_free(c);
		if (cnt == 0) {
			for (j = c.start_idx; j < c.end_idx; j++)
				cfl_literals.push_back(literals[j]);
			return SIMPLIFY_CONFLICT;
		} else if (cnt == -1) {
			// clause is satisfied, do nothing
		} else if (cnt == 1) {
			// clause is a unit clause, assign free variable
			unit_propagate(c);
			simpl_done = true;
		} else if (cnt > 1) {
			// clause has more than one free variable and is not sat
			sat = false;
		}
	}

	if (sat)
		return SIMPLIFY_SAT;
	else if (simpl_done)
		return SIMPLIFY_SOME;
	return SIMPLIFY_NONE;
}

const std::vector<cnf::var_state> *const cnf::solve()
{
	return solve(nullptr);
}

const std::vector<cnf::var_state> *const cnf::solve(bool *cancel)
{
	int8_t simplification_result;

	// reset the state vector
	state.clear();
	state.reserve(max_var+1);
	state.emplace_back(VAR_FORCED_FALSE); // 0 is a hardwired false variable
	while (state.size() < state.capacity())
		state.emplace_back(VAR_INIT);

	// reset the decision level vector
	decision_levels.clear();
	decision_levels.reserve(max_var+1);
	decision_levels.emplace_back(0); // 0 is a hardwired false variable
	while (decision_levels.size() < decision_levels.capacity())
		decision_levels.emplace_back(0);

	// reset the decision idx vector
	decision_idx.clear();
	decision_idx.reserve(max_var+1);
	decision_idx.emplace_back(0); // 0 is a hardwired false variable
	while (decision_idx.size() < decision_idx.capacity())
		decision_idx.emplace_back(0);

	// reset the decision vector
	decisions.clear();
	decisions.reserve(max_var);
	decision_level = 0;
	conflicts = 0;

	// perform the generic dpll algorithm
	while (true) {
		// check the cancellation token
		if (cancel != nullptr && *cancel)
			return NULL;
			
		// attempt to simplify
		simplification_result = check_clauses();
		switch (simplification_result) {
		case SIMPLIFY_CONFLICT:
			// if there is a conflict and we cannot backtrack, the problem is unsat
			if (!conflict())
				return NULL;
			break;
		case SIMPLIFY_NONE:
			// if we could not make a simplification, guess
			guess();
			break;
		case SIMPLIFY_SOME:
			// otherwise, just loop through and check clauses again
			break;
		case SIMPLIFY_SAT:
			// the expression is satisfiable, return the solution
			return &state;
		}
	}
}

int32_t cnf::clause_count_free(clause &c)
{
	int cnt = c.end_idx - c.start_idx;
	var_state *s;
	lit_t l;
	int i;

	// check if the clause is satisfied
	for (i = c.start_idx; i < c.end_idx; i++) {
		// read the state of the variable
		l = literals[i];
		s = &state[l.var];

		// if variable true and literal positive, then clause true
		if (s->assigned && s->assigned_true && !l.neg)
			return -1;

		// if variable false and literal negative, then clause true
		if (s->assigned && !s->assigned_true && l.neg)
			return -1;
	}

	// count the remaining free variables
	for (i = c.start_idx; i < c.end_idx; i++) {
		// read the state of the variable
		l = literals[i];
		s = &state[l.var];

		// variable is assigned and literal is false, reduce free count
		if (s->assigned)
			cnt--;

		// in all other cases, variable is free
	}

	return cnt;
}

void cnf::unit_propagate(clause &c)
{
	var_t v;
	var_state s;
	lit_t l;
	int i;

	// look for the only free variable in the provided clause
	i = c.start_idx;
	do {
		l = literals[i++];
		v = l.var;
	} while (state[l.var].assigned);

	// assign the variable
	s = state[v];
	s.assigned = true;
	s.assignment_forced = true;
	s.assigned_true = !l.neg;
	decide(v, s);
}

void cnf::guess()
{
	// TODO: Optimize
	// just search the whole variable list and make the first decision

	var_t v;
	var_state s;

	// search through the whole variable list
	for (v = 0; v < state.size(); v++) {
		s = state[v];
		if (!s.assigned)
			break;
	}

	// only ever decide false, backtrack makes a forced guess of true
	s.assigned = true;
	s.assigned_true = false;
	s.assignment_forced = false;
	decision_level += 1;
	decide(v, s);
}

void cnf::decide(var_t v, var_state s)
{
	state[v] = s;
	decision_levels[v] = decision_level;
	decision_idx[v] = decisions.size();
	decisions.push_back(v);
}

bool cnf::conflict()
{
	clause *ant;
	clause res;
	var_state s;
	var_t v;
	int i, j;

	// if the current decision level is 0, then all decisions were forces
	if (decision_level == 0)
		return false;

	// if assignment for l was not forced, just backtrack
	if (!state[decisions.back()].assignment_forced)
		return backtrack();

	// repeatedly resolve conflict with antecedent to find 1UIP
	while (!at_1uip())
		explain();

	// TODO: I believe this is impossible
	// simplify the conflict clause due to implication rules
	// for (i = 0; i < cfl_literals.size(); i++) {
	// 	for (j = i+1; j < cfl_literals.size(); j++) {
	// 		if (cfl_literals[i].var == cfl_literals[j].var
	// 		    	&& cfl_literals[i].neg != cfl_literals[j].neg) {
	// 			cfl_literals[i].var = 0;
	// 			cfl_literals[j].var = 0;
	// 		}
	// 	}
	// }

	// for (i = 0; i < cfl_literals.size(); i++) {
	// 	if (cfl_literals[i].var == 0)
	// 		cfl_literals.erase(cfl_literals.begin() + i);
	// }
	
	// TODO: Remove
	// learn the conflict clause
	// std::cerr << "Clause added to the conflict database: " << std::endl;
	res.start_idx = literals.size();
	for (i = 0; i < cfl_literals.size(); i++) {
		// if (cfl_literals[i].neg) std::cerr << "-";
		// std::cerr << cfl_literals[i].var << " ";
		literals.push_back(cfl_literals[i]);
	}
	// std::cerr << std::endl;
	res.end_idx = literals.size();
	clauses.push_back(res);

	// backjump after saving the clause
	return backjump();
}

bool cnf::at_1uip()
{
	uint32_t max_decision_level = 0;
	var_t v;
	int count = 0;
	int i;

	// find max decision level
	for (i = 0; i < cfl_literals.size(); i++) {
		v = cfl_literals[i].var;
		if (decision_levels[v] > max_decision_level)
			max_decision_level = decision_levels[v];
	}

	// if any variable in the max decision level is not forced, this is not 1uip
	for (i = 0; i < cfl_literals.size(); i++) {
		v = cfl_literals[i].var;
		if (decision_levels[v] == max_decision_level && state[v].assignment_forced)
			return false;
	}

	return true;
}

void cnf::explain()
{
	lit_t cfl_lit;
	var_t v;
	var_state s;
	clause *ant;
	uint32_t max_idx;
	int i, j, cfl_lit_itr;
	bool lit_in_clause;

	// detect the cfl_lit in the clause
	cfl_lit = VAR_POS(0);
	max_idx = 0;
	for (i = 0; i < cfl_literals.size(); i++) {
		if (decision_idx[cfl_literals[i].var] >= max_idx) {
			max_idx = decision_idx[cfl_literals[i].var];
			cfl_lit = cfl_literals[i];
			cfl_lit_itr = i;
		}
	}

	// TODO: Remove
	if (cfl_lit.var == 0)
		std::cerr << "uhhh\n";

	// TODO: I believe this is impossible
	// if cfl_lit is both pos and neg then resolve
	// for (i = cfl_lit_itr+1; i < cfl_literals.size(); i++) {
	// 	if (cfl_lit.var == cfl_literals[i].var && cfl_lit.neg != cfl_literals[i].neg) {
	// 		cfl_literals.erase(cfl_literals.begin() + i);
	// 		cfl_literals.erase(cfl_literals.begin() + cfl_lit_itr);
	// 		return;
	// 	}
	// }
	
	// detect a suitable anticedent clause
	for (clause &c : clauses) {
		ant = NULL;
		for (i = c.start_idx; i < c.end_idx; i++) {
			v = literals[i].var;
			s = state[v];
			if (v == cfl_lit.var && literals[i].neg != cfl_lit.neg) {
				ant = &c;
			} else if (!s.assigned || s.assigned_true != literals[i].neg) {
				// TODO: Remove
				// std::cerr << "test2\n";
				ant = NULL;
				break;
			}  else if (decision_idx[v] > max_idx) {
				// TODO: Remove
				// std::cerr << "test1\n";
				ant = NULL;
				break;
			}
		}

		if (ant != NULL)
			break;
	}

	// resolve the conflict clause with the anticedent
	for (i = 0; i < cfl_literals.size(); i++)
		if (cfl_literals[i].var == cfl_lit.var)
			break;
	cfl_literals.erase(cfl_literals.begin() + i);

	for (i = ant->start_idx; i < ant->end_idx; i++) {
		lit_in_clause = false;
		for (j = 0; j < cfl_literals.size(); j++)
			if (literals[i].var == cfl_literals[j].var && literals[i].neg == cfl_literals[j].neg)
				lit_in_clause = true;
		if (!lit_in_clause && literals[i].var != cfl_lit.var)
			cfl_literals.push_back(literals[i]);
	}
}

bool cnf::backtrack()
{
	var_t v;
	var_state s;

	// if the decision stack is empty, we cannot backtrack
	if (decisions.empty())
		return false;

	// undo decisions until the last guess
	do {
		v = decisions.back();
		s = state[v];
		decisions.pop_back();
		state[v] = VAR_INIT;
	} while (!decisions.empty() && s.assignment_forced);

	// if all of our decisions are forced, we cannot backtrack
	if (s.assignment_forced)
		return false;

	// conflict on guess, inversion is forced
	s.assignment_forced = true;
	s.assigned_true = !s.assigned_true;
	decision_level -= 1;
	decide(v, s);

	return true;
}

bool cnf::backjump()
{
	var_t v;
	var_state s;
	bool in_clause;
	lit_t cfl_lit;
	int max_decision_level = 0;
	int level;
	int i;

	// if the decision stack is empty, we cannot backtrack
	if (decisions.empty())
		return false;

	// compute second highest decision level in conflict clause
	for (i = 0; i < cfl_literals.size(); i++) {
		level = decision_levels[cfl_literals[i].var];
		if (level > max_decision_level) {
			decision_level = max_decision_level;
			max_decision_level = decision_levels[cfl_literals[i].var];
			cfl_lit = cfl_literals[i];
		} else if (level > decision_level && level < max_decision_level) {
			decision_level = level;
		}
	}

	// undo decisions up to the new decision level
	v = decisions.back();
	while (!decisions.empty() && decision_levels[decisions.back()] > decision_level) {
		v = decisions.back();
		decisions.pop_back();
		state[v] = VAR_INIT;
		decision_levels[v] = 0;
		decision_idx[v] = 0;
	}

	// assert resolution at the lowest possible decision level
	s.raw = 0;
	s.assigned = true;
	s.assigned_true = !cfl_lit.neg;
	s.assignment_forced = true;
	decide(cfl_lit.var, s);

	// we backtracked successfully, there is no longer conflict
	cfl_literals.clear();

	return true;
}

cnf::~cnf() = default;
