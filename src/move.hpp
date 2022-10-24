#pragma once

#include <qpl/qpl.hpp>
#include "state.hpp"
#include "info.hpp"
#include "access.hpp"

void perform_move(const qpl::filesys::path& source, const qpl::filesys::path& destination, const state& state) {
	if (info::find_ignored_root(source.ensured_directory_backslash())) {
		if (print_ignore && info::find_ignored(source.ensured_directory_backslash())) {
			if (state.print) {
				auto word = state.check_mode ? "[*]IGNORED " : "IGNORED ";
				qpl::println(qpl::str_lspaced(word, 40), source);
			}
		}
		info::checked.insert(destination);
		return;
	}

	if (source.is_directory()) {
		if (!destination.exists()) {
			info::move_changes = true;
			if (state.print) {
				auto word = state.check_mode ? "[*]NEW " : "NEW DIRECTORY ";
				qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_green, qpl::str_lspaced(word, 40), destination);
			}
			if (!state.check_mode) {
				destination.ensure_branches_exist();
			}
		}
		info::checked.insert(destination);
		return;
	}
	info::checked.insert(destination);

	if (destination.exists()) {
		//auto equals = source.file_equals_no_read(destination);
		auto equals = source.file_content_equals(destination);
		if (!equals) {
			auto fs1 = source.file_size();
			auto fs2 = destination.file_size();

			if (state.find_collisions) {
				auto time1 = source.last_write_time();
				auto time2 = destination.last_write_time();

				auto overwrites_newer = time2.time_since_epoch().count() > time1.time_since_epoch().count();

				bool different_time_but_same_data = overwrites_newer && source.file_content_equals(destination);

				if (overwrites_newer) {

					auto ns1 = std::chrono::duration_cast<std::chrono::nanoseconds>(time1.time_since_epoch()).count();
					auto ns2 = std::chrono::duration_cast<std::chrono::nanoseconds>(time2.time_since_epoch()).count();

					auto diff = qpl::time(ns2 - ns1).string_until_sec("");

					auto time_stamp = qpl::get_time_string(time2, "%Y-%m-%d %H-%M-%S");

					auto str = qpl::to_string(time_stamp, " ", qpl::str_spaced(qpl::to_string("( + ", diff, ")"), 30), " --- ", destination);

					if (different_time_but_same_data) {
						info::time_overwrites.push_back(str);
					}
					else {
						info::data_overwrites.push_back(str);
					}
				}
			}

			if (fs1 != fs2) {
				info::move_changes = true;
				auto diff = qpl::signed_cast(fs1) - qpl::signed_cast(fs2);
				if (state.print) {
					auto word = state.check_mode ? "[*]CHANGE" : "OVERWRITTEN";
					auto str = qpl::to_string(qpl::str_lspaced(qpl::to_string(word, " [", diff > 0 ? " + " : " - ", qpl::memory_size_string(qpl::abs(diff)), "] "), 40));
					qpl::println(state.check_mode ? qpl::color::white : diff > 0 ? qpl::color::light_green : qpl::color::light_red, str, destination);
				}

				if (!state.check_mode) {
					source.copy_overwrite(destination);
				}
			}
			else {
				info::move_changes = true;
				if (state.print) {
					auto word = state.check_mode ? "[*]CHANGE [BYTES CHANGED] " : "OVERWRITTEN [BYTES CHANGED] ";
					qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_green, qpl::str_lspaced(word, 40), destination);
				}
				if (!state.check_mode) {
					source.copy_overwrite(destination);
				}
			}
		}
	}
	else {
		if (state.print) {
			auto word = state.check_mode ? "[*]NEW " : "NEW ";
			qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_green, qpl::str_lspaced(word, 40), destination);
		}
		if (!state.check_mode) {
			source.copy(destination);
		}
	}
}

void clear_path(const qpl::filesys::path& path, const state& state) {
	if (info::find_ignored_root(path)) {
		if (print_ignore && info::find_ignored(path)) {
			if (state.print) {
				auto word = state.check_mode ? "[*]IGNORED " : "IGNORED ";
				qpl::println(qpl::str_lspaced(word, 40), path);
			}
		}
		return;
	}

	path.update();
	if (!path.exists()) {
		return;
	}

	if (!info::find_checked(path.ensured_directory_backslash())) {
		info::removes.push_back(path);
		if (state.print) {
			info::move_changes = true;
			auto word = state.check_mode ? "[*]REMOVE " : "REMOVED ";
			qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_red, qpl::str_lspaced(word, 40), path);
		}
		if (!state.check_mode) {
			qpl::filesys::remove(path.ensured_directory_backslash());
		}
	}
}

void clear_paths(const qpl::filesys::path& path, const state& state) {
	if (path.is_directory()) {
		auto paths = path.list_current_directory_tree_include_self();
		for (auto& path : paths) {
			clear_path(path, state);
		}
	}
	else {
		clear_path(path, state);
	}
}

void find_removables(const qpl::filesys::path& path, const state& state) {
	auto paths = path.list_current_directory();
	for (auto& path : paths) {
		path.ensure_directory_backslash();
		if (path.is_file() && info::find_checked(path)) {
			continue;
		}
		if (can_touch(path, state.action == action::push)) {
			clear_paths(path, state);
		}
		else {
			if (state.print && print_ignore) {
				auto word = state.check_mode ? "[*]IGNORED " : "IGNORED ";
				qpl::println(qpl::str_lspaced(word, 40), path);
			}
		}
	}
}

void move(const qpl::filesys::path& path, const state& state) {
	if (!path.exists()) {
		qpl::println("MOVE : ", path, " doesn't exist.");
		return;
	}
	if (!is_valid_working_directory(path)) {
		qpl::println("MOVE : ", path, " is not a valid working directory with a solution file.");
		return;
	}

	bool target_is_git = state.action == action::push;
	bool target_is_work = state.action == action::pull;
	auto branch = path.branch_size() - 1;
	std::string target_branch_name;

	qpl::filesys::path target_path;
	if (state.action == action::pull) {
		target_branch_name = path.get_directory_name();
		target_path = path.ensured_directory_backslash().with_branch(branch, "git");
	}
	else {
		target_branch_name = "git";
		target_path = path;
	}
	info::checked.clear();
	info::move_changes = false;

	auto paths = target_path.list_current_directory();
	for (auto& path : paths) {
		if (can_touch(path, target_is_work)) {
			if (path.is_directory()) {
				auto dir_paths = path.list_current_directory_tree_include_self();
				for (auto& path : dir_paths) {
					auto destination = path.with_branch(branch, target_branch_name).ensured_directory_backslash();
					perform_move(path, destination, state);
				}
			}
			else {
				auto destination = path.with_branch(branch, target_branch_name).ensured_directory_backslash();
				perform_move(path, destination, state);
			}
		}
		else {
			if (state.print && print_ignore) {
				auto word = state.check_mode ? "[*]IGNORED " : "IGNORED ";
				qpl::println(qpl::str_lspaced(word, 40), path);
			}
		}
	}

	auto git_path = path.ensured_directory_backslash().with_branch(branch, target_branch_name);
	find_removables(git_path, state);

	auto word = state.action == action::pull ? "PULL" : "PUSH";
	if (info::move_changes) {
		qpl::println("move [", word, "] status >> ", qpl::color::light_yellow, "directories have changed.");
	}
	else {
		qpl::println("move [", word, "] status >> ", qpl::color::light_green, "directories are synchronized.");
	}
}
