#pragma once

#include <qpl/qpl.hpp>
#include "state.hpp"

namespace info {
	std::unordered_set<std::string> checked;
	std::unordered_set<std::string> ignore;
	std::vector<std::string> data_overwrites;
	std::vector<std::string> time_overwrites;
	std::vector<std::string> removes;
	bool move_changes = false;

	void reset() {
		data_overwrites.clear();
		time_overwrites.clear();
		removes.clear();
	}
	bool find_checked(std::string str) {
		return checked.find(str) != checked.cend();
	}
	bool find_ignored_root(qpl::filesys::path path) {
		for (auto& i : info::ignore) {
			if (path.has_root(i)) {
				return true;
			}
		}
		return false;
	}
	bool find_ignored(qpl::filesys::path path) {
		for (auto& i : info::ignore) {
			if (path == qpl::filesys::path(i)) {
				return true;
			}
		}
		return false;
	}

}

constexpr bool print_ignore = false;