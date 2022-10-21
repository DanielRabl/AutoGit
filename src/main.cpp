#include <qpl/qpl.hpp>
#include <qpl/winsys.hpp>


namespace global {
	std::unordered_set<std::string> checked;
	std::unordered_set<std::string> ignore;
	std::vector<std::string> possible_recent_overwrites;
	std::vector<std::string> date_changed_overwrites;
	bool pull;
		 
	bool find_checked(std::string str) {
		return checked.find(str) != checked.cend();
	}
	bool find_ignored_root(qpl::filesys::path path) {
		for (auto& i : global::ignore) {
			if (path.has_root(i)) {
				return true;
			}
		}
		return false;
	}
	bool find_ignored(qpl::filesys::path path) {
		for (auto& i : global::ignore) {
			if (path == qpl::filesys::path(i)) {
				return true;
			}
		}
		return false;
	}
}

constexpr bool print_ignore = false;

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

bool can_touch(const qpl::filesys::path& path, bool is_git) {
	if (is_git) {
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

template<bool print, bool safe_mode, bool find_collisions>
void check_overwrite(const qpl::filesys::path& source, const qpl::filesys::path& destination) {
	if (global::find_ignored_root(source.ensured_directory_backslash())) {
		if (print_ignore && global::find_ignored(source.ensured_directory_backslash())) {
			if constexpr (print) {
				auto word = safe_mode ? "[*]IGNORED " : "IGNORED ";
				qpl::println(qpl::str_lspaced(word, 40), source);
			}
		}
		global::checked.insert(destination);
		return;
	}

	if (source.is_directory()) {
		if (!destination.exists()) {
			if constexpr (print) {
				auto word = safe_mode ? "[*]CREATED DIRECTORY  " : "CREATED DIRECTORY  ";
				qpl::set_console_color(qpl::color::light_green);
				qpl::println(qpl::str_lspaced(word, 40), destination);
				qpl::set_console_color_default();
			}
			if constexpr (!safe_mode) {
				destination.ensure_branches_exist();
			}
		}
		global::checked.insert(destination);
		return;
	}
	global::checked.insert(destination);

	if (destination.exists()) {
		auto equals = source.file_equals_no_read(destination);
		if (!equals) {
			auto fs1 = source.file_size();
			auto fs2 = destination.file_size();

			if constexpr (find_collisions) {
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
						global::date_changed_overwrites.push_back(str);
					}
					else {
						global::possible_recent_overwrites.push_back(str);
					}
				}
			}

			if (fs1 != fs2) {
				auto diff = qpl::signed_cast(fs1) - qpl::signed_cast(fs2);
				if constexpr (print) {
					auto word = safe_mode ? "[*]OVERWRITTEN" : "OVERWRITTEN";
					qpl::set_console_color(qpl::color::light_green);
					qpl::println(qpl::str_lspaced(qpl::to_string(word, " [", diff > 0 ? " + " : " - ", qpl::memory_size_string(qpl::abs(diff)), "] "), 40), destination);
					qpl::set_console_color_default();
				}

				if constexpr (!safe_mode) {
					source.copy_overwrite(destination);
				}
			}
			else {
				if constexpr (print) {
					auto word = safe_mode ? "[*]OVERWRITTEN [DATA CHANGED] " : "OVERWRITTEN [DATA CHANGED] ";
					qpl::set_console_color(qpl::color::light_green);
					qpl::println(qpl::str_lspaced(word, 40), destination);
					qpl::set_console_color_default();
				}
				if constexpr (!safe_mode) {
					source.copy_overwrite(destination);
				}
			}
		}
	}
	else {
		if constexpr (print) {
			auto word = safe_mode ? "[*]COPIED " : "COPIED ";
			qpl::set_console_color(qpl::color::light_green);
			qpl::println(qpl::str_lspaced(word, 40), destination);
			qpl::set_console_color_default();
		}
		if constexpr (!safe_mode) {
			source.copy(destination);
		}
	}
}

template<bool print, bool safe_mode, bool find_collisions>
void clear_path(const qpl::filesys::path& path) {
	if (global::find_ignored_root(path)) {
		if (print_ignore && global::find_ignored(path)) {
			if constexpr (print) {
				auto word = safe_mode ? "[*]IGNORED " : "IGNORED ";
				qpl::println(qpl::str_lspaced(word, 40), path);
			}
		}
		return;
	}

	path.update();
	if (!path.exists()) {
		return;
	}

	if (!global::find_checked(path.ensured_directory_backslash())) {
		if constexpr (print) {
			auto word = safe_mode ? "[*]REMOVED " : "REMOVED ";
			qpl::println(qpl::str_lspaced(word, 40), path);
		}
		if constexpr (!safe_mode) {
			qpl::filesys::remove(path.ensured_directory_backslash());
		}
	}
}

template<bool print, bool safe_mode, bool find_collisions>
void clear_paths(const qpl::filesys::path& path) {
	if (path.is_directory()) {
		auto paths = path.list_current_directory_tree_include_self();
		for (auto& path : paths) {
			clear_path<print, safe_mode, find_collisions>(path);
		}
	}
	else {
		clear_path<print, safe_mode, find_collisions>(path);
	}
}

template<bool print, bool safe_mode, bool find_collisions>
void find_removables(const qpl::filesys::path& path, bool is_git) {
	auto paths = path.list_current_directory();
	for (auto& path : paths) {
		path.ensure_directory_backslash();
		if (path.is_file() && global::find_checked(path)) {
			continue;
		}
		if (can_touch(path, is_git)) {
			clear_paths<print, safe_mode, find_collisions>(path);
		}
		else {
			if constexpr (print && print_ignore) {
				auto word = safe_mode ? "[*]IGNORED " : "IGNORED ";
				qpl::println(qpl::str_lspaced(word, 40), path);
			}
		}
	}
}

template<bool print, bool safe_mode, bool find_collisions>
void move(const qpl::filesys::path& path, bool pull_from_git) {
	if (!path.exists()) {
		qpl::println("MOVE : ", path, " doesn't exist.");
		return;
	}
	if (!is_valid_working_directory(path)) {
		qpl::println("MOVE : ", path, " is not a valid working directory with a solution file.");
		return;
	}

	bool is_target_git = !pull_from_git;
	auto branch = path.branch_size() - 1;
	std::string target_branch_name;

	qpl::filesys::path target_path;
	if (pull_from_git) {
		target_branch_name = path.get_directory_name();
		target_path = path.ensured_directory_backslash().with_branch(branch, "git");
	}
	else {
		target_branch_name = "git";
		target_path = path;
	}
	global::checked.clear();

	auto paths = target_path.list_current_directory();
	for (auto& path : paths) {
		if (can_touch(path, pull_from_git)) {
			if (path.is_directory()) {
				auto dir_paths = path.list_current_directory_tree_include_self();
				for (auto& path : dir_paths) {
					auto destination = path.with_branch(branch, target_branch_name).ensured_directory_backslash();
					check_overwrite<print, safe_mode, find_collisions>(path, destination);
				}
			}
			else {
				auto destination = path.with_branch(branch, target_branch_name).ensured_directory_backslash();
				check_overwrite<print, safe_mode, find_collisions>(path, destination);
			}
		}
		else {
			if constexpr (print && print_ignore) {
				auto word = safe_mode ? "[*]IGNORED " : "IGNORED ";
				qpl::println(qpl::str_lspaced(word, 40), path);
			}
		}
	}

	auto git_path = path.ensured_directory_backslash().with_branch(branch, target_branch_name);
	find_removables<print, safe_mode, find_collisions>(git_path, is_target_git);
}

template<bool print, bool safe_mode>
void git(const qpl::filesys::path& path, bool pull) {

	if constexpr (safe_mode) {
		return;
	}

	auto work_branch = path.branch_size() - 1;
	auto git_path = path.ensured_directory_backslash().with_branch(work_branch, "git");

	git_path.update();
	if (!git_path.exists()) {
		git_path = path;
	}

	qpl::filesys::path status_batch;
	std::string status_data;

	qpl::filesys::path exec_batch;
	std::string exec_data;

	auto same_dir = qpl::filesys::get_current_location().string().front() == git_path.string().front();
	auto set_directory = qpl::to_string(same_dir ? "cd " : "cd /D ", git_path);
	
	auto home = qpl::filesys::get_current_location().ensured_directory_backslash();

	auto output_file = home.appended("output.txt");
	output_file.create();

	qpl::filesys::path git_status = home.appended("git_status.bat");
	std::string git_status_data = qpl::to_string("@echo off && ", set_directory, " && @echo on && git status");

	if (pull) {
		status_batch = home.appended("git_pull_status.bat");
		status_data = qpl::to_string("@echo off && ", set_directory, " && git fetch && git status -uno > ", output_file);

		exec_batch = home.appended("git_pull.bat");
		exec_data = qpl::to_string("@echo off && ", set_directory, " && @echo on && git pull");
	}
	else {
		status_batch = home.appended("git_push_status.bat");
		status_data = qpl::to_string("@echo off && ", set_directory, " && git add -A && git status > ", output_file);

		exec_batch = home.appended("git_push.bat");
		exec_data = qpl::to_string("@echo off && ", set_directory, " && git commit -m \"update\" && git push");
	}

	qpl::filesys::create_file(status_batch, status_data);
	std::system(status_batch.c_str());
	qpl::filesys::remove(status_batch);

	auto lines = qpl::split_string(output_file.read(), '\n');
	if (lines.empty()) {
		qpl::println("error : no output from git status.");
		return;
	}

	bool clean_tree = false;
	if (pull) {
		if (1 < lines.size()) {
			std::string search = "Your branch is up to date";
			auto start = lines[1].substr(0u, search.length());
			clean_tree = qpl::string_equals_ignore_case(start, search);
		}
	}
	else {
		std::string search = "nothing to commit, working tree clean";
		clean_tree = qpl::string_equals_ignore_case(lines.back(), search);
	}

	if (clean_tree) {
		qpl::set_console_color(qpl::color::aqua);
		if (pull) {
			qpl::println("git status : branch is up-to-date.");
		}
		else {
			qpl::println("git status : nothing new to commit.");
		}
		qpl::set_console_color_default();
	}
	else {
		if (!pull) {
			qpl::filesys::create_file(git_status, git_status_data);
			std::system(git_status.c_str());
			qpl::filesys::remove(git_status);
		}

		qpl::filesys::create_file(exec_batch, exec_data);
		std::system(exec_batch.c_str());
		qpl::filesys::remove(exec_batch);
	}

	output_file.remove();
}
template<bool print, bool safe_mode, bool find_collisions>
void execute(const std::vector<std::string> lines, qpl::time& time_sum) {
	for (qpl::size i = 0u; i < lines.size(); ++i) {

		auto args = qpl::split_string_whitespace(lines[i]);
		if (args.empty()) {
			continue;
		}
		auto path = args.back();

		if (args.front().starts_with("//")) {
			continue;
		}

		if constexpr (print) {
			qpl::println();
			qpl::println_repeat("- ", 40);
			qpl::println();
		}

		auto dir_path = qpl::filesys::path(path).ensured_directory_backslash();

		if (global::pull && args.size() == 3u && args[0] == "MOVE" && args[1] == "GIT") {
			std::swap(args[0], args[1]);
		}
		else if (!global::pull && args.size() == 3u && args[0] == "GIT" && args[1] == "MOVE") {
			std::swap(args[0], args[1]);
		}

		if constexpr (print) {
			for (qpl::size i = 0u; i < args.size() - 1; ++i) {
				qpl::print(args[i], ' ');
			}
			qpl::println(dir_path);
		}

		for (qpl::size i = 0u; i < args.size() - 1; ++i) {
			const auto& command = args[i];
			if (command == "MOVE") {
				qpl::small_clock clock;
				move<print, safe_mode, find_collisions>(dir_path, global::pull);
				time_sum += clock.elapsed();
			}
			else if (command == "GIT") {
				qpl::small_clock clock;
				git<print, safe_mode>(dir_path, global::pull);
				time_sum += clock.elapsed();
			}
			else if (command == "IGNORE") {
				global::ignore.insert(dir_path);
			}
			else {
				if constexpr (print) {
					qpl::println("unkown command \"", command, "\" ignored.");
				}
			}
		}
	}
}

template<bool safe_mode>
bool confirm_collisions() {
	auto size = global::date_changed_overwrites.size();
	if (size) {
		qpl::println();
		qpl::println("HINT: there ", (size == 1 ? "is " : "are "), size, (size == 1 ? " file " : " files "), "that would overwrite a more recent version, but the data is same.");
		qpl::println();

		for (auto& i : global::date_changed_overwrites) {
			qpl::println(i);
		}
	}

	size = global::possible_recent_overwrites.size();
	if (size) {
		qpl::println();
		qpl::println("WARNING: there ", (size == 1 ? "is " : "are "), size, (size == 1 ? " file " : " files "), "that would overwrite a more recent version.");
		qpl::println();
		for (auto& i : global::possible_recent_overwrites) {
			qpl::println(i);
		}

		while (true) {
			qpl::println();
			qpl::println("Are you SURE you want to overwrite these files? (y/n)");
			qpl::print("> ");

			auto input = qpl::get_input();
			if (qpl::string_equals_ignore_case(input, "y")) {
				qpl::println();
				return true;
			}
			if (qpl::string_equals_ignore_case(input, "n")) {
				qpl::println();
				return false;
			}
		}
	}
	else {
		qpl::println("no collisions found.");
	}
	return true;
}

void determine_pull_or_push() {
	while (true) {
		qpl::print("PULL changes or PUSH changes > ");
		auto input = qpl::get_input();
		if (qpl::string_equals_ignore_case(input, "pull")) {
			global::pull = true;
			return;
		}
		else if (qpl::string_equals_ignore_case(input, "push")) {
			global::pull = false;
			return;
		}
		else {
			qpl::println("invalid command.\n");
		}
	}
}

void run() {
	auto lines = qpl::split_string(qpl::read_file("paths.cfg"), '\n');

	for (auto& line : lines) {
		qpl::remove_multiples(line, '\r');
	}
	while (true) {
		determine_pull_or_push();

		qpl::time time_sum = 0u;
		constexpr bool safe_mode = false;

		execute<false, true, true>(lines, time_sum);
		if (confirm_collisions<safe_mode>()) {
			execute<true, safe_mode, false>(lines, time_sum);
		}

		qpl::println();
		qpl::println_repeat("- ", 40);
		qpl::println();
		qpl::println("TOTAL : ", time_sum.string_until_ms());
		qpl::println();
		qpl::println_repeat("- ", 40);
		qpl::println_repeat("\n", 4);
	}
}

int main() try {
	run();
}
catch (std::exception& any) {

	//test

	qpl::println("caught exception:\n", any.what());
	qpl::system_pause();
}