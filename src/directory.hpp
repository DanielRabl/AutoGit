#pragma once
#include <qpl/qpl.hpp>
#include "state.hpp"
#include "access.hpp"
#include "move.hpp"
#include "exe.hpp"
#include "git.hpp"


struct directory {
	qpl::filesys::path solution_path;
	qpl::filesys::path git_path;
	qpl::filesys::path path;
	status push_status;
	status pull_status;

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

	std::vector<command> get_commands() const {
		if (this->is_git()) {
			return { command::git };
		}
		else if (this->is_solution()) {
			return { command::move, command::git };
		}
		return {};
	}
	std::vector<command> get_pull_commands() const {
		auto commands = this->get_commands();
		std::sort(commands.begin(), commands.end(), [&](auto a, auto b) {
			return a < b;
			});
		return commands;
	}
	std::vector<command> get_push_commands() const {
		auto commands = this->get_commands();
		std::sort(commands.begin(), commands.end(), [&](auto a, auto b) {
			return a > b;
			});
		return commands;
	}
	std::vector<command> get_commands(const state& state) const {
		if (state.action == action::push) {
			return this->get_pull_commands();
		}
		else if (state.action == action::pull) {
			return this->get_push_commands();
		}
		return {};
	}

	void exe(state state) {
		if (state.location != location::git) {
			if (state.status) {
				state.check_mode = true;
			}
			::exe(this->get_active_path(), state);
		}
	}
	void move(state state) {
		if (state.location != location::git) {
			if (state.status) {
				state.check_mode = true;
			}
			::move(this->get_active_path(), state);
		}
	}
	void git(const state& state) {
		if (state.location != location::local) {
			::git(this->get_active_path(), state);
		}
	}

	void execute(const state& state, command command) {
		bool git_print = state.print && command == command::git;
		bool move_print = state.print && command == command::move;

		if (git_print) {
			auto word = state.action == action::pull ? "PULL" : "PUSH";
			auto cw = state.action == action::pull ? qpl::color::light_green : qpl::color::light_blue;
			qpl::print(" git [", cw, word, "] status ");
		}
		if (move_print) {
			auto word = state.action == action::pull ? "PULL" : "PUSH";
			auto cw = state.action == action::pull ? qpl::color::light_green : qpl::color::light_blue;
			qpl::print("move [", cw, word, "] status ");
		}

		info::command_reset();
		switch (command) {
		case command::move:
			if (state.action == action::pull) {
				this->move(state);
				this->exe(state);
			}
			else if (state.action == action::push) {
				this->exe(state);
				this->move(state);
			}
			break;
		case command::git:
			this->git(state);
			break;
		}

		if (git_print && !info::git_changes) {
			auto word = state.action == action::pull ? "fetch" : "commit";
			qpl::println(qpl::color::light_yellow, qpl::to_string("nothing new to ", word, "."));
		}
		if (move_print && !info::move_changes) {
			qpl::println(qpl::color::light_yellow, "directories are synchronized.");
		}
		if (state.print && info::any_output) {
			qpl::println();
		}

	}
	static status get_status() {
		status status;
		status.git_changes = info::git_changes;
		status.local_changes = info::move_changes;
		status.local_collision = info::any_collisions();
		status.time_overwrites = !info::time_overwrites.empty();
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

	void print_commands(const state& state, const std::vector<command>& commands) {
		if (!this->any_active_commands(state, commands)) {
			return;
		}
		if (state.print) {
			for (qpl::size i = 0u; i < commands.size(); ++i) {
				auto active = this->is_command_active(state, commands[i]);
				qpl::color color = active ? qpl::color::bright_white : qpl::color::gray;
				qpl::print(color, command_string(commands[i]), ' ');
			}
			qpl::print(qpl::color::aqua, this->path);
		}
	}
	void println_commands(const state& state, const std::vector<command>& commands) {
		this->print_commands(state, commands);
		qpl::println();
	}
	void execute(const state& state, const std::vector<command>& commands) {
		if (!this->any_active_commands(state, commands)) {
			return;
		}

		info::reset();
		for (auto& command : commands) {
			this->execute(state, command);
		}
		this->determine_status(state);

		if (state.print) {
			print_collisions(state);
		}
	}

	static bool is_command_active(const state& state, command command) {
		if (command == command::move && state.location == location::git) {
			return false;
		}
		else if (command == command::exe && (state.location == location::git || state.action == action::pull)) {
			return false;
		}
		else if (command == command::git && state.location == location::local) {
			return false;
		}
		else {
			return true;
		}
	}
	static bool any_active_commands(const state& state, const std::vector<command>& commands) {
		for (auto command : commands) {
			if (is_command_active(state, command)) {
				return true;
			}
		}
		return false;
	}

	void execute(state state) {
		if (this->empty()) {
			return;
		}
		this->pull_status.reset();
		this->push_status.reset();

		if (state.action == action::both && (state.status || state.update)) {

			auto word = state.status ? "STATUS " : "UPDATE ";
			qpl::println('\n', word, qpl::color::aqua, this->path);

			auto check_mode = state.check_mode;
			state.print = true;
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
					print_collisions(state);
				}
				if (this->pull_status.time_overwrites) {
					state.action = action::pull;
					state.print = false;
					this->execute(state, this->get_commands(state));
					state.print = true;
					print_collisions(state);
				}
			}

			if (this->status_can_push()) {
				qpl::println("result: should ", info::special_color, "push", '.');
			}
			else if (this->status_can_pull()) {
				qpl::println("result: should ", info::special_color, "pull", '.');
			}
			else if (this->status_has_conflicts()) {
				qpl::println('\n', qpl::color::light_red, "CONFLICT summary: ", this->status_conflict_string());
			}

			if (state.update) {
				state.check_mode = check_mode;
				state.status = false;
				state.print = true;

				if (this->status_can_push()) {
					state.action = action::push;
					auto commands = this->get_commands(state);

					this->execute(state, this->get_commands(state));
				}
				else if (this->status_can_pull()) {
					state.action = action::pull;
					auto commands = this->get_commands(state);

					this->execute(state, this->get_commands(state));
				}
			}
		}
		else {
			auto commands = this->get_commands(state);
			this->println_commands(state, commands);
			this->execute(state, commands);

			if (state.print || state.find_collisions) {
				print_collisions(state);
			}
		}

	}
};