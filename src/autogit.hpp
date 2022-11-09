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

	void execute(const state& state) {
		for (auto& dir : this->directories) {
			bool valid = state.target_input_directories.empty() || (state.target_input_directories.size() && qpl::find(state.target_input_directories, dir.path));
			if (valid) {
				dir.execute(state);
			}
		}
	}
};