#pragma once

#include "autogit_directory.hpp"
#include <qpl/qpl.hpp>

struct autogit {
	std::vector<autogit_directory> directories;

	void find_directory(qpl::filesys::path path, bool recursive = true) {
		if (path.string().starts_with("//")) {
			return;
		}
		path.ensure_directory_backslash();
		if (!path.exists()) {
			qpl::println('\\', path, "\\ doesn't exist.");
		}

		autogit_directory directory;
		directory.set_path(path);
		if (directory.empty() && recursive) {
			auto list = path.list_current_directory();
			for (auto& path : list) {
				this->find_directory(path, false);
			}
		}
		else {
			this->directories.push_back(directory);
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
		if (state.action != action::both && !state.status && !state.update) {
			auto collision_state = state;

			collision_state.find_collisions = true;
			collision_state.print = false;
			collision_state.check_mode = true;
			collision_state.only_collisions = true;

			this->execute_no_collisions(collision_state);
			return confirm_collisions(collision_state);
		}
		return true;
	}

	void execute(const state& state) {
		qpl::clock timer;

		auto confirm = this->execute_check_collisions(state);

		timer.pause();
		if (confirm) {
			timer.resume();
			this->execute_no_collisions(state);
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