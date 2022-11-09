#pragma once
#include <qpl/qpl.hpp>
#include "state.hpp"
#include "access.hpp"
#include "move.hpp"
#include "exe.hpp"
#include "git.hpp"
#include "collisions.hpp"


struct directory {
	qpl::filesys::path solution_path;
	qpl::filesys::path git_path;
	qpl::filesys::path path;
	status push_status;
	status pull_status;
	history_status history;
	bool pulled = false;

	bool is_git() const {
		return this->solution_path.empty() && !this->git_path.empty();
	}
	bool is_solution() const {
		return !this->solution_path.empty() && !this->git_path.empty();
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
		if (this->is_git() && state.mode.location != location::local) {
			return { command::git };
		}
		if (this->is_solution()) {
			if (state.mode.location == location::local) {
				return { command::move };
			}
			else if (state.mode.location == location::git) {
				return { command::git };
			}
			else if (state.mode.location == location::both) {
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
		if (state.mode.action == action::push) {
			return this->get_pull_commands(state);
		}
		else if (state.mode.action == action::pull) {
			return this->get_push_commands(state);
		}
		return {};
	}

	void exe(state state) {
		if (state.mode.location != location::git) {
			if (state.mode.status) {
				state.mode.check_mode = true;
			}
			::exe(this->get_active_path(), state, this->history);
		}
	}
	void move(state state) {
		if (state.mode.location != location::git) {
			if (state.mode.status) {
				state.mode.check_mode = true;
			}
			::move(this->get_active_path(), state, this->history);
		}
	}
	void git(const state& state) {
		if (state.mode.location != location::local) {
			::git(this->get_active_path(), state, this->history);
			if (state.mode.action == action::pull && this->history.git_changes) {
				this->pulled = true;
			}
		}
	}

	void execute(const state& state, command command) {
		if (command == command::move && this->pulled) {
			auto collision_state = state;
			collision_state.mode.find_collisions = true;
			collision_state.mode.print = false;
			collision_state.mode.check_mode = true;
			collision_state.mode.only_collisions = true;

			this->execute(collision_state, command::move);
			if (!confirm_collisions(collision_state)) {
				return;
			}
		}

		bool git_print = command == command::git;
		bool move_print = command == command::move;

		auto word = state.mode.action == action::pull ? "PULL" : "PUSH";
		auto status_word = state.mode.status ? "status" : "update status";
		if (!state.mode.only_collisions && git_print) {
			auto cw = state.mode.action == action::pull ? qpl::color::light_green : qpl::color::aqua;
			qpl::print(cw, "------", " git [", cw, word, "] ", status_word, " ");
		}
		if (!state.mode.only_collisions && move_print) {
			auto cw = state.mode.action == action::pull ? qpl::color::light_green : qpl::color::aqua;
			qpl::print(cw, "-----", " move [", cw, word, "] ", status_word, " ");
		}

		//state.command_reset();
		this->history.command_reset();
		switch (command) {
		case command::move:
			if (state.mode.action == action::pull) {
				this->move(state);
				this->exe(state);
			}
			else if (state.mode.action == action::push) {
				this->exe(state);
				this->move(state);
			}
			break;
		case command::git:
			this->git(state);
			break;
		}

		if (!state.mode.only_collisions && git_print) {
			auto word = state.mode.action == action::pull ? "fetch" : "commit";
			if (!this->history.git_changes) {
				qpl::println(qpl::color::gray, qpl::to_string("nothing new to ", word, "."));
			}
			else if (state.mode.update && this->history.git_changes && state.mode.status) {
				qpl::println(qpl::color::light_yellow, qpl::to_string("needs git ", word, "."));
			}
		}
		if (!state.mode.only_collisions && move_print) {
			if (!this->history.move_changes) {
				qpl::println(qpl::color::gray, "directories are synchronized.");
			}
			else if (state.mode.update && this->history.move_changes && state.mode.status) {
				qpl::println(qpl::color::light_yellow, "directories are changed.");
			}
		}
		if (this->history.any_output) {
			qpl::println();
		}

		if (state.mode.only_collisions) {
			if (info::any_collisions()) {
				qpl::println("COLLISIONS ", qpl::color::aqua, this->path);
				print_collisions(state);
			}
		}
	}
	status get_status() {
		status status;
		status.git_changes = this->history.git_changes;
		status.local_changes = this->history.move_changes;
		status.local_collision = info::any_serious_collisions();
		status.time_overwrites = !info::time_overwrites.empty();
		return status;
	}
	void determine_status(const state& state) {
		if (state.mode.action == action::pull) {
			this->pull_status = this->get_status();
		}
		else if (state.mode.action == action::push) {
			this->push_status = this->get_status();
		}
	}
	bool status_clean() const {
		return (this->push_status.clean() && this->pull_status.clean());
	}
	bool status_can_push() const {
		return this->push_status.can_execute() && !this->pull_status.git_changes;
	}
	bool status_can_pull() const {
		return this->pull_status.can_execute() && !this->push_status.git_changes;
	}
	bool status_has_conflicts() const {
		if (this->status_can_pull() || this->status_can_push()) {
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
		info::reset();
		this->history.reset();
		//state.reset();
		for (auto& command : commands) {
			this->execute(state, command);
		}
		this->determine_status(state);

		if (state.mode.print) {
			print_collisions(state);
		}
	}

	void execute(state state) {
		if (this->empty()) {
			return;
		}
		this->pull_status.reset();
		this->push_status.reset();
		this->pulled = false;

		if (state.mode.action == action::both && (state.mode.status || state.mode.update)) {
			if (this->get_pull_commands(state).empty() && this->get_push_commands(state).empty()) {
				return;
			}

			if (!state.mode.only_collisions) {
				auto word = state.mode.status ? "STATUS " : "UPDATE ";
				qpl::println('\n', word, qpl::color::aqua, this->path);
			}

			auto check_mode = state.mode.check_mode;
			state.mode.print = state.mode.status;
			state.mode.status = true;
			state.mode.check_mode = true;
			state.mode.find_collisions = true;

			state.mode.action = action::push;
			this->execute(state, this->get_commands(state));
			state.mode.action = action::pull;
			this->execute(state, this->get_commands(state));

			if (!state.mode.print) {
				if (this->push_status.time_overwrites) {
					state.mode.action = action::push;
					state.mode.print = false;
					this->execute(state, this->get_commands(state));
					state.mode.print = true;
					print_collisions(state);
				}
				if (this->pull_status.time_overwrites) {
					state.mode.action = action::pull;
					state.mode.print = false;
					this->execute(state, this->get_commands(state));
					state.mode.print = true;
					print_collisions(state);
				}
			}

			if (!state.mode.only_collisions) {
				if (this->status_can_push()) {
					qpl::println("can safely ", qpl::color::aqua, "push", '.');
				}
				else if (this->status_can_pull()) {
					qpl::println("can safely ", qpl::color::light_green, "pull", '.');
				}
				else if (this->status_has_conflicts()) {
					qpl::println('\n', qpl::color::light_red, "CONFLICT summary: ", this->status_conflict_string());
				}
			}

			if (state.mode.update) {
				state.mode.check_mode = check_mode;
				state.mode.status = false;
				state.mode.print = true;

				if (this->status_can_push()) {
					state.mode.action = action::push;
					auto commands = this->get_commands(state);

					qpl::println();
					this->execute(state, this->get_commands(state));
				}
				else if (this->status_can_pull()) {
					state.mode.action = action::pull;
					auto commands = this->get_commands(state);

					qpl::println();
					this->execute(state, this->get_commands(state));
				}
			}
		}
		else {
			auto commands = this->get_commands(state);
			if (commands.empty()) {
				return;
			}

			if (!state.mode.only_collisions) {
				auto word = state.mode.status ? "STATUS " : "UPDATE ";
				qpl::println('\n', word, qpl::color::aqua, this->path);
			}
			this->execute(state, this->get_commands(state));
		}

	}
};