#include <qpl/qpl.hpp>


void check_overwrite(const qpl::filesys::path& source, qpl::size git_branch_index, std::unordered_set<std::string>& checked) {
	auto destination = source.with_branch(git_branch_index, "git");

	if (source.is_directory()) {
		destination.append("/");
		if (!destination.exists()) {
			qpl::println(qpl::str_lspaced("CREATED DIRECTORY ", 40), destination);
			destination.ensure_branches_exist();
		}
		checked.insert(destination);
		return;
	}
	checked.insert(destination);

	if (destination.exists()) {
		auto fs1 = source.file_size();
		auto fs2 = destination.file_size();
		if (fs1 != fs2) {
			auto diff = qpl::signed_cast(fs1) - qpl::signed_cast(fs2);
			qpl::println(qpl::str_lspaced(qpl::to_string("OVERWRITTEN [", diff > 0 ? "+" : "-", qpl::memory_size_string(qpl::abs(diff)), "] "), 40), destination);
			source.copy_overwrite(destination);
		}
		else {
			auto str1 = source.read();
			auto str2 = destination.read();

			if (str1 != str2) {
				qpl::println(qpl::str_lspaced("OVERWRITTEN[DIFFERENT DATA] ", 40), destination);
				source.copy_overwrite(destination);
			}
		}
	}
	else {
		qpl::println(qpl::str_lspaced("COPIED ", 40), destination);
		source.copy(destination);
	}
}

void move_files(const qpl::filesys::path& path) {
	auto git_branch = path.branch_size() - 1;

	std::unordered_set<std::string> checked;
	auto paths = path.list_current_directory();
	for (auto& path : paths) {
		if (path.is_file()) {

			bool valid_file = true;
			auto name = path.get_full_file_name();
			auto split = qpl::split_string(name, ".");

			if (split.size() == 2u && split[1] == "vcxproj") {
				valid_file = false;
			}
			else if (split.size() == 3u && split[1] == "vcxproj" && split[2] == "filters") {
				valid_file = false;
			}
			else if (split.size() == 3u && split[1] == "vcxproj" && split[2] == "user") {
				valid_file = false;
			}
			if (valid_file) {
				check_overwrite(path, git_branch, checked);
			}
			else {
				qpl::println(qpl::str_lspaced("IGNORED ", 40), path);
			}
		}
		if (path.is_directory()) {
			bool valid_dir = true;

			auto dir_name = path.get_directory_name();
			if (dir_name == "Debug") {
				valid_dir = false;
			}
			else if (dir_name == "Release") {
				valid_dir = false;
			}
			else if (dir_name == "x64") {
				valid_dir = false;
			}

			if (valid_dir) {
				auto dir_paths = path.list_current_directory_tree();
				check_overwrite(path, git_branch, checked);
				for (auto& i : dir_paths) {
					check_overwrite(i, git_branch, checked);
				}
			}
			else {
				qpl::println(qpl::str_lspaced("IGNORED ", 40), path);
			}
		}
	}

	auto git_path = path.ensured_directory_backslash().with_branch(git_branch, "git");
	paths = git_path.list_current_directory_tree();

	for (auto& path : paths) {
		path.ensure_directory_backslash();
		if (checked.find(path) == checked.cend()) {
			bool valid = false;
			if (path.is_directory() && path.get_directory_name() == ".git") {
				valid = true;
			}
			if (!valid) {
				qpl::println(qpl::str_lspaced("REMOVE ", 40), path);
				qpl::filesys::remove(path);
			}
		}
	}
}

int main() try {
	auto path = qpl::filesys::get_current_location();
	move_files(path);

	qpl::config config;
	config.load("paths.cfg");

	qpl::println(config.get(0u));
}
catch (std::exception& any) {
	qpl::println("caught exception:\n", any.what());
	qpl::system_pause();
}