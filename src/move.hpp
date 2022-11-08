#pragma once

#include <qpl/qpl.hpp>
#include "state.hpp"
#include "info.hpp"
#include "access.hpp"

std::string time_diff_string(std::filesystem::file_time_type time1, std::filesystem::file_time_type time2, bool show_time_stamp) {

	auto ns1 = std::chrono::duration_cast<std::chrono::nanoseconds>(time1.time_since_epoch()).count();
	auto ns2 = std::chrono::duration_cast<std::chrono::nanoseconds>(time2.time_since_epoch()).count();

	bool negative = ns2 < ns1;
	if (negative) {
		std::swap(ns1, ns2);
		std::swap(time1, time2);
	}

	auto diff = qpl::time(ns2 - ns1).string_short("");
	auto time_stamp = qpl::get_time_string(time2, "%Y-%m-%d %H-%M-%S");
	auto diff_str = qpl::to_string("( ", negative ? '-' : '+', ' ', diff, " )");
	if (show_time_stamp) {
		return qpl::to_string(time_stamp, ' ', diff_str);
	}
	else {
		return diff_str;
	}
}

void perform_move(const qpl::filesys::path& source, const qpl::filesys::path& destination, const state& state) {
	if (info::find_ignored_root(source.ensured_directory_backslash())) {
		if (print_ignore && info::find_ignored(source.ensured_directory_backslash())) {
			if (state.print) {
				if (!info::any_output) qpl::println();
				auto word = state.check_mode ? "[*]IGNORED " : "IGNORED ";
				qpl::println(qpl::str_lspaced(word, info::print_space), source);
				info::any_output = true;
			}
		}
		info::checked.insert(destination);
		return;
	}

	if (source.is_directory()) {
		if (!destination.exists()) {
			info::move_changes = true;
			if (state.print) {
				if (!info::any_output) qpl::println();

				auto word = state.check_mode ? "[*]NEW   " : "NEW DIR";
				auto str = qpl::str_lspaced(qpl::to_string(word, " + ", qpl::memory_size_string(source.file_size_recursive())), info::print_space);
				qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_green, str, destination);
				info::any_output = true;
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
		bool equals;
		if (state.quick_mode) {
			equals = source.file_equals_no_read(destination);
		}
		else {
			equals = source.file_equals(destination);
		}
		if (!equals) {
			auto fs1 = source.file_size();
			auto fs2 = destination.file_size();

			auto time1 = source.last_write_time();
			auto time2 = destination.last_write_time();

			if (state.find_collisions) {
				auto overwrites_newer = time2.time_since_epoch().count() > time1.time_since_epoch().count();

				bool different_time_but_same_data = overwrites_newer && source.file_content_equals(destination);

				if (overwrites_newer) {
					auto str = qpl::to_string(qpl::str_lspaced(time_diff_string(time1, time2, true), 42), " --- ", destination);

					if (different_time_but_same_data) {
						info::time_overwrites.push_back(str);
						++info::total_change_sum;
					}
					else {
						info::data_overwrites.push_back(str);
						++info::total_change_sum;
					}
				}
			}

			if (fs1 != fs2) {
				info::move_changes = true;
				auto diff = qpl::signed_cast(fs1) - qpl::signed_cast(fs2);
				if (state.print) {
					if (!info::any_output) qpl::println();

					auto word = state.check_mode ? "[*]MODIFY" : "MODIFIED";
					auto str = qpl::to_string(qpl::str_lspaced(qpl::to_string(word, diff > 0 ? " + " : " - ", qpl::memory_size_string(qpl::abs(diff))), info::print_space));
					qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_green, str, destination);
					info::any_output = true;
				}

				if (!state.check_mode) {
					source.copy_overwrite(destination);
				}
			}
			else if (time1 != time2) {
				info::move_changes = true;
				if (state.print) {
					if (!info::any_output) qpl::println();
					auto diff_str = time_diff_string(time1, time2, false);

					auto word = state.check_mode ? "[*]MODIFY TIME" : "MODIFIED TIME";
					auto str = qpl::to_string(word, ' ', diff_str);
					qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_green, qpl::str_lspaced(str, info::print_space), destination);
					info::any_output = true;
				}
				if (!state.check_mode) {
					source.copy_overwrite(destination);
				}
			}
			else {
				info::move_changes = true;
				if (state.print) {
					if (!info::any_output) qpl::println();

					auto word = state.check_mode ? "[*]MODIFY [BYTES CHANGED] " : "MODIFIED [BYTES CHANGED] ";
					qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_green, qpl::str_lspaced(word, info::print_space), destination);
					info::any_output = true;
				}
				if (!state.check_mode) {
					source.copy_overwrite(destination);
				}
			}
		}
	}
	else {
		info::move_changes = true;
		if (state.print) {
			if (!info::any_output) qpl::println();
			auto word = state.check_mode ? "[*]NEW   " : "ADDED  ";
			auto str = qpl::str_lspaced(qpl::to_string(word, " + ", qpl::memory_size_string(source.file_size_recursive())), info::print_space);
			qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_green, str, destination);
			info::any_output = true;
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
				if (!info::any_output) qpl::println();
				auto word = state.check_mode ? "[*]IGNORED" : "IGNORED";
				qpl::println(qpl::str_lspaced(word, info::print_space), path);
				info::any_output = true;
			}
		}
		return;
	}

	path.update();
	if (!path.exists()) {
		return;
	}

	if (!info::find_checked(path.ensured_directory_backslash())) {
		info::move_changes = true;
		info::removes.push_back(path);
		++info::total_change_sum;
		if (state.print) {
			if (!info::any_output) qpl::println();

			auto word = state.check_mode ? "[*]REMOVE" : "REMOVED";
			auto str = qpl::str_lspaced(qpl::to_string(word, " - ", qpl::memory_size_string(path.file_size_recursive())), info::print_space);
			qpl::println(state.check_mode ? qpl::color::white : qpl::color::light_red, str, path);
			info::any_output = true;
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
				qpl::println(qpl::str_lspaced(word, info::print_space), path);
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
	if (!has_git_directory(path.get_parent_branch())) {
		qpl::println("MOVE : ", path, " couldn't find a git directory.");
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

					auto destination = path.with_branch(branch, target_branch_name);
					if (path.is_directory()) {
						destination.ensure_backslash();
					}
					perform_move(path, destination, state);
				}
			}
			else {
				auto destination = path.with_branch(branch, target_branch_name).ensured_directory_backslash();
				if (path.is_directory()) {
					destination.ensure_backslash();
				}
				perform_move(path, destination, state);
			}
		}
		else {
			if (state.print && print_ignore) {
				auto word = state.check_mode ? "[*]IGNORED " : "IGNORED ";
				qpl::println(qpl::str_lspaced(word, info::print_space), path);
			}
		}
	}

	auto git_path = path.ensured_directory_backslash().with_branch(branch, target_branch_name);
	find_removables(git_path, state);
}
