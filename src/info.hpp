#pragma once

#include <qpl/qpl.hpp>
#include "state.hpp"

namespace info {
	std::unordered_set<std::string> checked;
	std::unordered_set<std::string> ignore;

	qpl::size total_change_sum = 0u;
	std::vector<std::string> data_overwrites;
	std::vector<std::string> time_overwrites;
	std::vector<std::string> removes;

	constexpr auto print_space = 40;

	bool any_serious_collisions() {
		return data_overwrites.size() || removes.size();
	}
	bool any_collisions() {
		return any_serious_collisions() || time_overwrites.size();
	}
	void total_reset() {
		total_change_sum = 0u;
	}
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