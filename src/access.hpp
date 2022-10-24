#pragma once

#include <qpl/qpl.hpp>

bool can_touch_working_file(const qpl::filesys::path& path) {
	if (!path.is_file()) {
		return true;
	}
	auto name = path.get_full_file_name();
	auto split = qpl::split_string(name, ".");

	if (split.size() == 2u && split[1] == "vcxproj") {
		return false;
	}
	else if (split.size() == 3u && split[1] == "vcxproj" && split[2] == "filters") {
		return false;
	}
	else if (split.size() == 3u && split[1] == "vcxproj" && split[2] == "user") {
		return false;
	}
	return true;
}

bool can_touch_working_directory(const qpl::filesys::path& path) {
	if (!path.is_directory()) {
		return true;
	}
	auto dir_name = path.get_directory_name();

	if (dir_name == "Debug") {
		return false;
	}
	else if (dir_name == "Release") {
		return false;
	}
	else if (dir_name == "x64") {
		return false;
	}
	return true;
}

bool can_touch_working(const qpl::filesys::path& path) {
	return can_touch_working_directory(path) && can_touch_working_file(path);
}

bool can_touch_git_directory(const qpl::filesys::path& path) {
	if (path.is_directory() && path.get_directory_name() == ".git") {
		return false;
	}
	return true;
}

bool can_touch(const qpl::filesys::path& path, bool push) {
	if (push) {
		return can_touch_git_directory(path);
	}
	else {
		return can_touch_working(path);
	}
}

bool is_valid_working_directory(const qpl::filesys::path& path) {
	auto list = path.list_current_directory();
	auto project_name = path.get_directory_name();
	for (auto& path : list) {
		if (path.is_file()) {
			if (path.get_file_name() == project_name && path.get_file_extension() == "vcxproj") {
				return true;
			}
		}
	}
	return false;
}