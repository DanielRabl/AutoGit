#include <qpl/qpl.hpp>

namespace global {
	std::unordered_set<std::string> checked;
	std::unordered_set<std::string> ignore;
	std::vector<std::string> possible_recent_overwrites;
	std::string git_bash_path;
		 
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

constexpr bool print_ignore = true;



bool can_touch_working_file(const qpl::filesys::path& path) {
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
bool can_touch_git_directory(const qpl::filesys::path& path) {
	if (path.is_directory() && path.get_directory_name() == ".git") {
		return false;
	}
	return true;
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


template<bool print, bool safe_mode, bool find_overwrites>
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
				qpl::println(qpl::str_lspaced(word, 40), destination);
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
		auto equals = source.file_content_equals(destination);
		if (!equals) {
			auto fs1 = source.file_size();
			auto fs2 = destination.file_size();

			if constexpr (find_overwrites) {

				auto time1 = source.last_write_time();
				auto time2 = destination.last_write_time();

				auto overwrites_newer = time2.time_since_epoch().count() > time1.time_since_epoch().count();

				if (overwrites_newer) {

					auto ns1 = std::chrono::duration_cast<std::chrono::nanoseconds>(time1.time_since_epoch()).count();
					auto ns2 = std::chrono::duration_cast<std::chrono::nanoseconds>(time2.time_since_epoch()).count();

					auto diff = qpl::time(ns2 - ns1).string_until_sec("");

					auto time_stamp = qpl::get_time_string(time2, "%Y-%m-%d %H-%M-%S");

					auto str = qpl::to_string(time_stamp, " ", qpl::str_spaced(qpl::to_string("( + ", diff, ")"), 30), " --- ", destination);
					global::possible_recent_overwrites.push_back(str);
				}
			}

			if (fs1 != fs2) {
				auto diff = qpl::signed_cast(fs1) - qpl::signed_cast(fs2);
				if constexpr (print) {
					auto word = safe_mode ? "[*]OVERWRITTEN" : "OVERWRITTEN";
					qpl::println(qpl::str_lspaced(qpl::to_string(word, " [", diff > 0 ? " + " : " - ", qpl::memory_size_string(qpl::abs(diff)), "] "), 40), destination);
				}

				if constexpr (!safe_mode) {
					source.copy_overwrite(destination);
				}
			}
			else {
				if constexpr (print) {
					auto word = safe_mode ? "[*]OVERWRITTEN [DIFFERENT DATA] " : "OVERWRITTEN [DIFFERENT DATA] ";
					qpl::println(qpl::str_lspaced(word, 40), destination);
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
			qpl::println(qpl::str_lspaced(word, 40), destination);
		}
		if constexpr (!safe_mode) {
			source.copy(destination);
		}
	}
}


template<bool print, bool safe_mode, bool find_overwrites>
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
template<bool print, bool safe_mode, bool find_overwrites>
void clear_paths(const qpl::filesys::path& path) {
	if (path.is_directory()) {
		auto paths = path.list_current_directory_tree_include_self();
		for (auto& path : paths) {
			clear_path<print, safe_mode, find_overwrites>(path);
		}
	}
	else {
		clear_path<print, safe_mode, find_overwrites>(path);
	}
}

template<bool print, bool safe_mode, bool find_overwrites>
void work_to_git(const qpl::filesys::path& path) {
	if (!path.exists()) {
		qpl::println("WORK_TO_GIT : ", path, " doesn't exist.");
		return;
	}
	if (!is_valid_working_directory(path)) {
		qpl::println("WORK_TO_GIT : ", path, " is not a valid working directory.");
		return;
	}
	auto git_branch = path.branch_size() - 1;
	global::checked.clear();

	auto paths = path.list_current_directory();
	for (auto& path : paths) {
		if (path.is_file()) {

			if (can_touch_working_file(path)) {
				auto destination = path.with_branch(git_branch, "git").ensured_directory_backslash();
				check_overwrite<print, safe_mode, find_overwrites>(path, destination);
			}
			else {
				if constexpr (print && print_ignore) {
					auto word = safe_mode ? "[*]IGNORED " : "IGNORED ";
					qpl::println(qpl::str_lspaced(word, 40), path);
				}
			}
		}
		else if (path.is_directory()) {
			bool valid_dir = true;

			if (can_touch_working_directory(path)) {
				auto dir_paths = path.list_current_directory_tree_include_self();
				for (auto& path : dir_paths) {
					auto destination = path.with_branch(git_branch, "git").ensured_directory_backslash();
					check_overwrite<print, safe_mode, find_overwrites>(path, destination);
				}
			}
			else {
				if constexpr (print && print_ignore) {
					auto word = safe_mode ? "[*]IGNORED " : "IGNORED ";
					qpl::println(qpl::str_lspaced(word, 40), path);
				}
			}
		}
	}

	auto git_path = path.ensured_directory_backslash().with_branch(git_branch, "git");
	paths = git_path.list_current_directory();

	for (auto& path : paths) {
		path.ensure_directory_backslash();

		if (!global::find_checked(path)) {
			if (can_touch_git_directory(path)) {
				clear_paths<print, safe_mode, find_overwrites>(path);
			}
			else {
				if constexpr (print && print_ignore) {
					auto word = safe_mode ? "[*]IGNORED " : "IGNORED ";
					qpl::println(qpl::str_lspaced(word, 40), path);
				}
			}
		}
	}
}

template<bool print, bool safe_mode, bool find_overwrites>
void git_to_work(const qpl::filesys::path& path) {
	if (!path.exists()) {
		qpl::println("GIT_TO_WORK : ", path, " doesn't exist.");
		return;
	}
	if (!is_valid_working_directory(path)) {
		qpl::println("GIT_TO_WORK : ", path, " is not a valid working directory.");
		return;
	}
	auto work_branch = path.branch_size() - 1;
	auto project_name = path.get_directory_name();

	auto git_path = path.ensured_directory_backslash().with_branch(work_branch, "git");
	global::checked.clear();

	auto paths = git_path.list_current_directory();
	for (auto& path : paths) {
		if (path.is_file()) {
			auto destination = path.with_branch(work_branch, project_name).ensured_directory_backslash();
			check_overwrite<print, safe_mode, find_overwrites>(path, destination);
		}
		else if (path.is_directory()) {
			bool valid_dir = true;

			if (can_touch_git_directory(path)) {
				auto dir_paths = path.list_current_directory_tree_include_self();
				for (auto& path : dir_paths) {
					auto destination = path.with_branch(work_branch, project_name).ensured_directory_backslash();
					check_overwrite<print, safe_mode, find_overwrites>(path, destination);
				}
			}
			else {
				if constexpr (print && print_ignore) {
					auto word = safe_mode ? "[*]IGNORED " : "IGNORED ";
					qpl::println(qpl::str_lspaced(word, 40), path);
				}
			}
		}
	}

	auto work_path = git_path.ensured_directory_backslash().with_branch(work_branch, project_name);
	paths = work_path.list_current_directory();

	for (auto& path : paths) {
		path.ensure_directory_backslash();

		if (!global::find_checked(path)) {
			if (path.is_file()) {
				if (can_touch_working_file(path)) {
					clear_paths<print, safe_mode, find_overwrites>(path);
				}
				else {
					if constexpr (print && print_ignore) {
						auto word = safe_mode ? "[*]IGNORED " : "IGNORED ";
						qpl::println(qpl::str_lspaced(word, 40), path);
					}
				}
			}
			else if (path.is_directory()) {
				if (can_touch_working_directory(path)) {
					clear_paths<print, safe_mode, find_overwrites>(path);
				}
				else {
					if constexpr (print && print_ignore) {
						auto word = safe_mode ? "[*]IGNORED " : "IGNORED ";
						qpl::println(qpl::str_lspaced(word, 40), path);
					}
				}
			}
		}
	}
}

template<bool print, bool safe_mode>
void git_push(const qpl::filesys::path& path) {
	if constexpr (!safe_mode) {
		return;
	}

	if (global::git_bash_path.empty()) {
		qpl::println("git_push ", path, " : no GIT_BASH specified.");
		return;
	}

	auto command = qpl::to_string("\"", global::git_bash_path, "\" --dir ", path);
	qpl::println("command = ", command);
	system(command.c_str());
}

template<bool print, bool safe_mode, bool find_overwrites>
void execute(const std::vector<std::string> lines, qpl::time& time_sum) {
	bool first = true;
	for (qpl::size i = 0u; i < lines.size(); ++i) {

		auto args = qpl::split_string_whitespace(lines[i]);
		if (args.size() != 2) {

			if (args.size() > 0u && qpl::string_equals_ignore_case(args[0], "GITBASH")) {
				for (qpl::size c = 0u; c < lines[i].size(); ++c) {
					if (lines[i][c] == '"') {
						global::git_bash_path = lines[i].substr(c + 1);
						if constexpr (print) {
							qpl::println("GIT_BASH at ", global::git_bash_path);
						}
						if (global::git_bash_path.back() == '"') {
							global::git_bash_path.pop_back();
						}
						break;
					}
				}
			}

			continue;
		}
		auto command = args[0];
		auto path = args[1];

		if (command.starts_with("//")) {
			continue;
		}

		if (!first) {
			if constexpr (print) {
				qpl::println();
				qpl::println_repeat("- ", 50);
				qpl::println();
			}
		}
		first = false;


		auto dir_path = qpl::filesys::path(path).ensured_directory_backslash();
		if constexpr (print) {
			qpl::println(command, " ", dir_path);
		}

		if (command == "WORK_TO_GIT") {
			qpl::small_clock clock;
			work_to_git<print, safe_mode, find_overwrites>(dir_path);
			auto elapsed = clock.elapsed();
			if constexpr (print) {
				qpl::println("Took ", elapsed.string_until_ms());
			}

			time_sum += elapsed;
		}
		else if (command == "GIT_TO_WORK") {
			qpl::small_clock clock;
			git_to_work<print, safe_mode, find_overwrites>(dir_path);
			auto elapsed = clock.elapsed();
			if constexpr (print) {
				qpl::println("Took ", elapsed.string_until_ms());
			}

			time_sum += elapsed;
		}
		else if (command == "GIT_PUSH") {
			qpl::small_clock clock;
			git_push<print, safe_mode>(dir_path);
			auto elapsed = clock.elapsed();
			if constexpr (print) {
				qpl::println("Took ", elapsed.string_until_ms());
			}

			time_sum += elapsed;
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

bool confirm_overwrites() {
	auto size = global::possible_recent_overwrites.size();
	if (size) {
		qpl::println("there ", (size == 1 ? "is " : "are "), size, (size == 1 ? " file " : " files "), "that would overwrite a more recent version.");
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
				return true;
			}
			if (qpl::string_equals_ignore_case(input, "n")) {
				return false;
			}
		}
	}
	else {
		qpl::println("no problems found.");
		qpl::println();
	}
	return true;
}

int main() try {
	auto lines = qpl::split_string(qpl::read_file("paths.cfg"), '\n');

	for (auto& line : lines) {
		qpl::remove_multiples(line, '\r');
	}

	qpl::time time_sum = 0u;

	execute<false, true, true>(lines, time_sum);
	if (confirm_overwrites()) {
		execute<true, false, false>(lines, time_sum);
	}

	qpl::println();
	qpl::println("TOTAL : ", time_sum.string_until_ms());
	qpl::system_pause();
}
catch (std::exception& any) {
	qpl::println("caught exception:\n", any.what());
	qpl::system_pause();
}