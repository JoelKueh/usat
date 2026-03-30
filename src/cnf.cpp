#include "cnf.h"

#include <iostream>
#include <fstream>
#include <sstream>

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
	uint32_t i;
	clause c;

	// FIX: detection for pure literal elimination is totally broken
	// reset the appears_pos and appears_neg flags in each clause
	for (var_state &s : state) {
		s.appears_neg = false;
		s.appears_pos = false;
	}

	// check all clauses
	for (i = 0; i < clauses.size(); i++) {
		c = clauses[i];
		cnt = clause_count_free(c);
		if (cnt == 0) {
			// clause is empty, backtrack
			// TODO: Remove
			// std::cerr << "Clause: " << i << std::endl;
			// for (i = c.start_idx; i < c.end_idx; i++) {
			// 	std::cerr << "Var: " << literals[i].var << ", Neg: " << literals[i].neg << std::endl;
			// }
			// std::cerr << std::endl;
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

	// perform pure literal elimination for all variables
	// for (v = 0; v < state.size(); v++) {
	// 	s = state[v];
	// 	if (!s.assigned && s.appears_pos && !s.appears_neg) {
	// 		decide(v, VAR_FORCED_TRUE);
	// 		simpl_done = true;
	// 	} else if (!s.assigned && !s.appears_pos && s.appears_neg) {
	// 		decide(v, VAR_FORCED_FALSE);
	// 		simpl_done = true;
	// 	}
	// }

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

	// reset the decision vector
	decisions.clear();
	decisions.reserve(max_var);

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
			if (!backtrack())
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

		// book keeping for pure literal elimination
		s->appears_pos = s->appears_pos || !l.neg;
		s->appears_neg = s->appears_neg || l.neg;

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
	s.assignment_forced = true; // deduction: assignment is not a guess (forced)
	s.assigned_true = !l.neg;   // if the literal is negated, assign false, else assign true
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
	decide(v, s);
}

void cnf::decide(var_t v, var_state s)
{
	state[v] = s;
	decisions.push_back(v);
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
	decide(v, s);

	return true;
}

cnf::~cnf() = default;
