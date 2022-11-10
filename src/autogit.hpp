#pragma once

#include "autogit_directory.hpp"
#include <qpl/qpl.hpp>

struct autogit {
	std::vector<autogit_directory> directories;

	void find_directory(qpl::filesys::path path) {
		if (path.string().starts_with("//")) {
			return;
		}
		path.ensure_directory_backslash();
		if (!path.exists()) {
			qpl::println('\\', path, "\\ doesn't exist.");
		}

		autogit_directory directory;
		directory.set_path(path);
		if (directory.empty()) {
			if (path.is_directory() && !directory.is_solution_without_git()) {
				auto list = path.list_current_directory();
				for (auto& path : list) {
					this->find_directory(path);
				}
			}
		}
		else {
			this->directories.push_back(directory);
		}
	}
	void print() {
		qpl::size length = 0u;
		for (auto& i : this->directories) {
			if (!i.empty()) {
				length = qpl::max(length, i.directory_name.length());
			}
		}
		for (auto& i : this->directories) {
			if (!i.empty()) {
				qpl::println(qpl::appended_to_string_to_fit(qpl::to_string(i.directory_name, ' '), "- ", length + 3), qpl::color::aqua, i.path);
			}
		}
	}
	void find_directories(const std::vector<std::string>& location) {
		this->directories.clear();
		for (auto& i : location) {
			this->find_directory(i);
		}
	}

	void execute_no_collisions(const state& state) {
		for (auto& dir : this->directories) {
			bool valid = state.target_input_directories.empty() || (state.target_input_directories.size() && qpl::find(state.target_input_directories, dir.path));
			if (valid) {
				dir.execute(state);
			}
		}
	}
	bool execute_check_collisions(const state& state) {
		auto collision_state = state;

		collision_state.find_collisions = true;
		collision_state.print = false;
		collision_state.check_mode = true;
		collision_state.only_conflicts = true;

		this->execute_no_collisions(collision_state);
		return confirm_collisions(collision_state);
	}

	void execute(const state& state) {
		qpl::clock timer;

		bool needs_check = state.action != action::both && !state.status && !state.update;
		if (needs_check) {
			if (this->execute_check_collisions(state)) {
				this->execute_no_collisions(state);
			}
		}
		else {
			this->execute_no_collisions(state);
			if (state.action == action::both && state.status) {
				qpl::size found_moves = 0u;
				for (auto& dir : this->directories) {
					if (dir.can_do_safe_move()) {
						++found_moves;
					}
				}
				if (found_moves) {
					bool update = false;
					while (true) {

						qpl::println('\n');

						qpl::size length = 0u;
						for (auto& dir : this->directories) {
							if (dir.can_do_safe_move()) {
								length = qpl::max(length, dir.path.string().length());
							}
						}
						for (auto& dir : this->directories) {
							if (dir.can_do_safe_move()) {

								qpl::print(">>> ");
								auto word = dir.can_safely_pull ? "pull" : "push";
								qpl::println(qpl::color::aqua, qpl::appended_to_string_to_fit(dir.path, ". ", length), " update: ", qpl::color::aqua, word, '.');
							}
						}
						auto word = found_moves == 1 ? "directory" : "directories";
						qpl::print("\nwould you like to update ", qpl::color::aqua, found_moves, ' ', word, "? (y / n) > ");
						auto input = qpl::get_input();
						if (qpl::string_equals_ignore_case(input, "y")) {
							update = true;
							break;
						}
						else if (qpl::string_equals_ignore_case(input, "n")) {
							update = false;
							break;
						}
						qpl::println("\"", input, "\" invalid input");
					}
					if (update) {
						auto update_state = state;
						update_state.update = true;
						for (auto& dir : this->directories) {
							if (dir.can_do_safe_move()) {
								dir.perform_safe_move(update_state);
							}
						}
					}
				}
			}
		}

		if (timer.elapsed_f() > 10.0) {
			qpl::println('\n');
			auto str = qpl::to_string("time : ", timer.elapsed().string_short());
			qpl::println_repeat("-", str.length());
			qpl::println(str);
			qpl::println_repeat("-", str.length());
		}
		qpl::println('\n');
	}
};