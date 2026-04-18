#pragma once

#include <string>
#include <cstdint>
#include <vector>

#define VAR_POS(x) ((lit_t){.neg = false, .var = x})
#define VAR_NEG(x) ((lit_t){.neg = true, .var = x})

#define LIT_NEG_BIT 0b001

// A CNF SAT problem consists of a set of clauses.
class cnf
{
	public:
		typedef uint32_t var_t;
		struct clause {
			size_t start_idx;
			size_t end_idx;
		};
		union lit_t {
			struct {
				bool neg : 1;
				var_t var : 31;
			};
			uint32_t raw;
		};
		union var_state {
			struct {
				bool assigned : 1;          // Is this variable assigned?
				bool assigned_true : 1;     // Has this variable assigned true?
				bool assignment_forced : 1; // Was this assignment forced?
			};
			uint8_t raw;
		};

		cnf();
		int init(std::string cnf_path);

		inline uint32_t get_max_var() { return max_var; };
		inline uint32_t get_clause_cnt() { return clauses.size(); };
		inline uint32_t get_lit_cnt() { return literals.size(); };

		// @brief Solves the cnf sat problem.
		// @return A pointer to the solution if satisfiable, null otherwise
		const std::vector<var_state> *const solve();
		const std::vector<var_state> *const solve(bool *cancel);
		~cnf();

	private:
		std::vector<clause> clauses;  // The list of clauses that we maintain
		std::vector<lit_t> literals;    // Pool of literals for the clauses
		std::vector<var_state> state; // State vector with variable assignments
		std::vector<uint32_t> decision_levels; // vector of decision levels for variables
		std::vector<uint32_t> decision_idx; // vector of decision indicies for variables
		std::vector<var_t> decisions;    // Decision stack with assignment history
		std::vector<lit_t> cfl_literals;
		uint32_t var_cnt;
		uint32_t max_var;
		uint32_t clause_cnt;
		uint32_t decision_level;


		typedef uint8_t clause_state;
		enum simplification_result {
			SIMPLIFY_CONFLICT = -1,
			SIMPLIFY_NONE = 0,
			SIMPLIFY_SOME = 1,
			SIMPLIFY_SAT = 2,
		};

		const var_state VAR_INIT = var_state {.raw = 0};
		const var_state VAR_FORCED_TRUE = var_state {
			.assigned = 1,
			.assigned_true = 1,
			.assignment_forced = 1,
		};
		const var_state VAR_FORCED_FALSE = var_state {
			.assigned = 1,
			.assigned_true = 0,
			.assignment_forced = 1,
		};

		// @brief Counts the number of free variables in the current state.
		// Returns -1 if the clause is satisfied in the current state.
		// @param c The clause to check.
		// @return The number of free variables or -1 if satisfied.
		int32_t clause_count_free(clause &c);

		// @brief Checks all clauses and simplifies where possible.
		// @return -1 if unsat, 0 if simplification impossible, 1 if simplification possible
		int8_t check_clauses();

		// @biref Make a heuristics-guided assignment to a variable.
		void guess();

		// @brief Performs unit propagation on viable clause 'c'.
		void unit_propagate(clause &c);
		
		// @brief Makes a decision 's' about a variable 'v'.
		void decide(var_t v, var_state s);

		// @brief Handles inference when a conflict is detected.
		// @return true if the conflict could be resolved else false.
		bool conflict();

		// @brief Detects if the conflict clause is at the 1 unique implication point.
		// @return true if the conflict clause has only one decision at the current level.
		bool at_1uip();

		// @breif Resolves the current conflict clause with an antecedent.
		void explain();

		// @brief Undoes decisions up to and including the last guess.
		// @return false if we cannot backtrack (unsat), otherwise true
		bool backtrack();

		// @brief Undoes decisions up to and including the last guess.
		// @return false if we cannot backjump (unsat), otherwise true
		bool backjump();
};
