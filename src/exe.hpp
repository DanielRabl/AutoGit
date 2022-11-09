#pragma once

#include <qpl/qpl.hpp>
#include "info.hpp"

std::optional<qpl::filesys::path> get_most_recent_exe(const qpl::filesys::path& path) {
	auto parent = path.get_parent_branch();
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
		return std::nullopt;
	}

	qpl::sort(executables, [](auto a, auto b) {
		return a.first < b.first;
		});
	return executables.back().second;
}

void exe(const qpl::filesys::path& path, const state& state, history_status& history) {
	if (state.mode.action != action::push) {
		return;
	}
	auto parent = path.get_parent_branch();
	auto target_name = parent.get_directory_name();
	auto project_name = path.get_directory_name();

	auto optional_latest = get_most_recent_exe(path);
	if (!optional_latest.has_value()) {
		return;
	}
	auto target_exe = optional_latest.value();

	auto destination = parent.appended(qpl::to_string(project_name, '/', target_name, ".exe"));

	if (destination.exists() && target_exe.file_content_equals(destination)) {
		return;
	}

	history.move_changes = true;
	if (state.mode.print) {
		if (!history.any_output) qpl::println();

		std::string word;
		if (destination.exists()) {
			word = state.mode.check_mode ? "[*]MODIFY .exe" : "MODIFIED .exe";
		}
		else {
			word = state.mode.check_mode ? "[*]NEW .exe" : "ADDED .exe";
		}
		qpl::println(state.mode.check_mode ? qpl::color::white : qpl::color::light_green, qpl::str_lspaced(word, info::print_space), destination);
		history.any_output = true;
	}

	if (!state.mode.check_mode) {
		target_exe.copy_overwrite(destination);
	}
}