#pragma once

#include <qpl/qpl.hpp>
#include "info.hpp"

void exe(const qpl::filesys::path& path, const state& state) {
	if (state.action != action::push) {
		return;
	}
	auto parent = path.get_parent_branch();
	auto target_name = parent.get_directory_name();
	auto project_name = path.get_directory_name();

	std::vector<std::pair<qpl::size, qpl::filesys::path>> executables;

	auto check_search = [&](qpl::filesys::path search) {
		if (search.exists()) {
			executables.push_back(std::make_pair(search.last_write_time().time_since_epoch().count(), search));
		}
	};
	check_search(parent.appended(qpl::to_string("x64/Release/", project_name, ".exe")));
	check_search(parent.appended(qpl::to_string("x64/Debug/", project_name, ".exe")));
	check_search(parent.appended(qpl::to_string(project_name, "/Release/", project_name, ".exe")));
	check_search(parent.appended(qpl::to_string(project_name, "/Debug/", project_name, ".exe")));

	if (executables.empty()) {
		return;
	}

	qpl::sort(executables, [](auto a, auto b) {
		return a.first < b.first;
	});
	auto destination = parent.appended(qpl::to_string(project_name, '/', target_name, ".exe"));
	auto best = executables.back().second;

	if (state.print) {
		std::string word;
		if (destination.exists()) {
			word = state.check_mode ? "[*]CHANGE" : "CHANGED";
		}
		else {
			word = state.check_mode ? "[*]NEW" : "NEW";
		}
		qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_green, qpl::str_lspaced(word, 40), destination);
	}

	if (!state.check_mode) {
		best.copy_overwrite(destination);
	}
}