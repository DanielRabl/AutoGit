#pragma once
#include <qpl/qpl.hpp>
#include "state.hpp"
#include "access.hpp"
#include "move.hpp"
#include "exe.hpp"
#include "git.hpp"
#include "collisions.hpp"


struct autogit_directory {
	qpl::filesys::path solution_path;
	qpl::filesys::path git_path;
	qpl::filesys::path path;
	std::string directory_name;
	status push_status;
	status pull_status;
	history_status history;
	bool pulled = false;
	bool can_safely_push = false;
	bool can_safely_pull = false;

	bool is_git() const {
		return this->solution_path.empty() && !this->git_path.empty();
	}
	bool is_solution() const {
		return !this->solution_path.empty() && !this->git_path.empty();
	}
	bool is_solution_without_git() const {
		return !this->solution_path.empty() && this->git_path.empty();
	}
	bool empty() {
		return !this->is_git() && !this->is_solution();
	}
	void print() {
		if (this->is_git()) {
			qpl::println(this->git_path, " is a git directory.");
		}
		else if (this->is_solution()) {
			qpl::println(this->solution_path, " is a solution directory.");
			qpl::println(this->git_path, " is the git path.");
		}
		else {
			qpl::println(this->path, " is a solution directory.");
		}
	}
	void set_path(const qpl::filesys::path& path) {
		this->solution_path.clear();
		this->git_path.clear();

		this->path = path.ensured_directory_backslash();
		auto solution_optional = get_solution_directory_if_valid(this->path);
		auto git_optional = get_git_directory_if_valid(this->path);
		if (solution_optional.has_value()) {
			this->solution_path = solution_optional.value();

			auto git = get_git_directory_if_valid(this->path);
			if (git.has_value()) {
				this->git_path = git.value();
			}
		}
		else if (is_git_directory(this->path)) {
			this->git_path = this->path;
		}
		this->directory_name = this->path.ensured_directory_backslash().get_directory_name();
		if (this->directory_name == "git") {
			this->directory_name = this->path.get_parent_branch().get_directory_name();
		}
	}
	qpl::filesys::path get_active_path() const {
		if (this->is_git()) {
			return this->git_path;
		}
		else if (this->is_solution()) {
			return this->solution_path;
		}
		return {};
	}

	std::vector<command> get_unsorted_commands(const state& state) const {
		if (this->is_git() && state.location != location::local) {
			return { command::git };
		}
		if (this->is_solution()) {
			if (state.location == location::local) {
				return { command::move };
			}
			else if (state.location == location::git) {
				return { command::git };
			}
			else if (state.location == location::both) {
				return { command::move, command::git };
			}
		}
		return {};
	}
	std::vector<command> get_pull_commands(const state& state) const {
		auto commands = this->get_unsorted_commands(state);
		std::sort(commands.begin(), commands.end(), [&](auto a, auto b) {
			return a < b;
			});
		return commands;
	}
	std::vector<command> get_push_commands(const state& state) const {
		auto commands = this->get_unsorted_commands(state);
		std::sort(commands.begin(), commands.end(), [&](auto a, auto b) {
			return a > b;
			});
		return commands;
	}
	std::vector<command> get_commands(const state& state) const {
		if (state.action == action::push) {
			return this->get_pull_commands(state);
		}
		else if (state.action == action::pull) {
			return this->get_push_commands(state);
		}
		return {};
	}

	void perform_exe(state state) {
		if (state.location != location::git) {
			if (state.status) {
				state.check_mode = true;
			}
			::exe(this->get_active_path(), state, this->history);
		}
	}
	void perform_move(state state) {
		if (state.location != location::git) {
			if (state.status) {
				state.check_mode = true;
			}
			::move(this->get_active_path(), state, this->history);
		}
	}
	void perform_git(const state& state) {
		if (state.location != location::local) {
			::git(this->get_active_path(), state, this->history);
			if (state.action == action::pull && this->history.git_changes) {
				this->pulled = true;
			}
		}
	}

	void execute(const state& state, command command) {

		auto actual_update = state.update && !state.status && !state.check_mode && !state.only_conflicts;
		if (actual_update && command == command::move && this->pulled) {
			auto collision_state = state;
			collision_state.find_collisions = true;
			collision_state.print = false;
			collision_state.check_mode = true;
			collision_state.only_conflicts = true;

			this->execute(collision_state, command::move);
			if (!confirm_collisions(collision_state)) {
				return;
			}
		}

		bool git_print = command == command::git;
		bool move_print = command == command::move;

		auto word = state.action == action::pull ? "PULL" : "PUSH";
		auto status_word = state.status ? "status" : "update status";
		if (!state.only_conflicts && git_print) {
			auto cw = state.action == action::pull ? qpl::color::light_green : qpl::color::aqua;
			qpl::print(cw, "------", " git [", cw, word, "] ", status_word, " ");
		}
		if (!state.only_conflicts && move_print) {
			auto cw = state.action == action::pull ? qpl::color::light_green : qpl::color::aqua;
			qpl::print(cw, "-----", " move [", cw, word, "] ", status_word, " ");
		}

		this->history.command_reset();
		switch (command) {
		case command::move:
			if (state.action == action::pull) {
				this->perform_move(state);
				this->perform_exe(state);
			}
			else if (state.action == action::push) {
				this->perform_exe(state);
				this->perform_move(state);
			}
			break;
		case command::git:
			this->perform_git(state);
			break;
		}

		if (!state.only_conflicts && git_print) {
			auto word = state.action == action::pull ? "fetch" : "commit";
			if (!this->history.git_changes) {
				qpl::println(qpl::color::gray, qpl::to_string("nothing new to ", word, "."));
			}
			else if (state.update && this->history.git_changes && state.status) {
				qpl::println(qpl::color::light_yellow, qpl::to_string("needs git ", word, "."));
			}
		}
		if (!state.only_conflicts && move_print) {
			if (!this->history.move_changes) {
				qpl::println(qpl::color::gray, "directories are synchronized.");
			}
			else if (state.update && this->history.move_changes && state.status) {
				qpl::println(qpl::color::light_yellow, "directories are changed.");
			}
		}

		if (this->history.any_output) {
			qpl::println();
		}
	}
	status get_status() {
		status status;
		status.git_changes = this->history.git_changes;
		status.local_changes = this->history.move_changes;
		status.local_collision = this->history.any_serious_collisions();
		status.time_overwrites = !this->history.time_overwrites.empty();
		return status;
	}
	void determine_status(const state& state) {
		if (state.action == action::pull) {
			this->pull_status = this->get_status();
		}
		else if (state.action == action::push) {
			this->push_status = this->get_status();
		}
	}
	bool status_clean() const {
		return (this->push_status.clean() && this->pull_status.clean());
	}
	bool status_can_push() const {
		return this->push_status.can_execute() && !this->pull_status.git_changes;
	}
	bool status_can_push_both_changes() const {
		return this->push_status.both_changes() && !this->pull_status.git_changes;
	}
	bool status_can_pull() const {
		return this->pull_status.can_execute() && !this->push_status.git_changes;
	}
	bool status_can_pull_both_changes() const {
		return this->pull_status.both_changes() && !this->push_status.git_changes;
	}
	bool status_has_conflicts() const {
		if (this->status_can_pull() || this->status_can_push()) {
			return false;
		}
		if (this->status_can_push_both_changes() || this->status_can_pull_both_changes()) {
			return false;
		}
		return !this->status_clean();
	}
	std::string status_conflict_string() const {
		std::ostringstream stream;
		if (this->push_status.can_execute() && this->pull_status.just_git_changes()) {
			stream << qpl::to_string(" could push, but there is something to GIT pull.");
		}
		else if (this->pull_status.can_execute() && this->push_status.just_git_changes()) {
			stream << qpl::to_string(" could pull, but there is something to GIT push.");
		}
		else {
			stream << "\nPUSH status:\n" << this->push_status.string("push") << "\nPULL status:\n" << this->pull_status.string("pull");
		}
		return stream.str();
	}
	void execute(const state& state, const std::vector<command>& commands) {
		this->history.reset();

		for (auto& command : commands) {
			this->execute(state, command);
		}
		this->determine_status(state);

		auto actual_update = state.update && !state.status;
		if (state.only_conflicts && this->history.any_collisions()) {
			qpl::println("COLLISIONS ", qpl::color::aqua, this->path);
			print_collisions(state, this->history);
		}
		else if (state.print && !actual_update) {
			print_collisions(state, this->history);
		}
	}
	void perform_safe_move(state state) {
		if (!this->can_do_safe_move()) {
			return;
		}
		state.status = false;
		state.print = true;

		auto can_push_both_changes = this->status_can_push_both_changes();
		auto can_pull_both_changes = this->status_can_pull_both_changes();
		if (can_push_both_changes || can_pull_both_changes) {
			auto word = can_push_both_changes ? "push" : "pull";
			while (true) {
				qpl::print("do you want to overwrite the git directory changes after ", can_push_both_changes ? "local " : "git ", qpl::color::aqua, word, " (y / n) > ");
				auto input = qpl::get_input();
				if (qpl::string_equals_ignore_case(input, "y")) {
					break;
				}
				else if (qpl::string_equals_ignore_case(input, "n")) {
					return;
				}
			}
		}

		if (this->status_can_push() || can_push_both_changes) {
			state.action = action::push;
			auto commands = this->get_commands(state);

			qpl::println();
			this->execute(state, this->get_commands(state));
		}
		else if (this->status_can_pull() || can_pull_both_changes) {
			state.action = action::pull;
			auto commands = this->get_commands(state);

			qpl::println();
			this->execute(state, this->get_commands(state));
		}
	}
	bool can_do_safe_move() const {
		return this->can_safely_pull || this->can_safely_push;
	}
	void execute(state state) {
		if (this->empty()) {
			return;
		}
		this->pull_status.reset();
		this->push_status.reset();
		this->pulled = false;
		this->can_safely_push = false;
		this->can_safely_pull = false;

		if (state.action == action::both && (state.status || state.update)) {
			if (this->get_pull_commands(state).empty() && this->get_push_commands(state).empty()) {
				return;
			}

			if (!state.only_conflicts) {
				auto word = state.status ? "STATUS " : "UPDATE ";
				qpl::println('\n', word, qpl::color::aqua, this->path);
			}

			auto check_mode = state.check_mode;
			state.print = state.status;
			state.status = true;
			state.check_mode = true;
			state.find_collisions = true;

			state.action = action::push;
			this->execute(state, this->get_commands(state));
			state.action = action::pull;
			this->execute(state, this->get_commands(state));

			if (!state.print) {
				if (this->push_status.time_overwrites) {
					state.action = action::push;
					state.print = false;
					this->execute(state, this->get_commands(state));
					state.print = true;
					print_collisions(state, this->history);
				}
				if (this->pull_status.time_overwrites) {
					state.action = action::pull;
					state.print = false;
					this->execute(state, this->get_commands(state));
					state.print = true;
					print_collisions(state, this->history);
				}
			}

			if (!state.only_conflicts) {
				auto can_push = this->status_can_push();
				auto can_pull = this->status_can_pull();
				auto can_push_both_changes = this->status_can_push_both_changes();
				auto can_pull_both_changes = this->status_can_pull_both_changes();

				this->can_safely_pull = can_pull || can_pull_both_changes;
				this->can_safely_push = can_push || can_push_both_changes;

				if (this->can_safely_pull && this->can_safely_push) {
					this->can_safely_pull = can_pull;
					this->can_safely_push = can_push;
				}

				if (can_push || can_pull) {
					auto word = can_push ? "push" : "pull";
					if (!this->history.any_output) qpl::println();
					qpl::println(".-----------------.");
					qpl::println("| can safely ", qpl::color::aqua, word, " |");
					qpl::println(".-----------------.");
				}
				if (can_push_both_changes || can_pull_both_changes) {
					auto word = can_push_both_changes ? "push" : "pull";
					if (!this->history.any_output) qpl::println();

					qpl::println(".-------------------------------------------------------.");
					qpl::println("| can ", qpl::color::aqua, word, ", but would overwrite changes in the git folder | ");
					qpl::println(".-------------------------------------------------------.");
				}
				else if (this->status_has_conflicts()) {
					qpl::println(qpl::color::light_red, "CONFLICT summary: ", this->status_conflict_string());
				}
			}

			if (state.update) {
				state.check_mode = check_mode;
				this->perform_safe_move(state);
			}
		}
		else {
			auto commands = this->get_commands(state);
			if (commands.empty()) {
				return;
			}
			if (!state.only_conflicts) {
				auto word = state.status ? "STATUS " : "UPDATE ";
				qpl::println('\n', word, qpl::color::aqua, this->path);
			}
			this->execute(state, this->get_commands(state));
		}

	}
};