#pragma once

#include <qpl/qpl.hpp>
#include "info.hpp"
#include "batch.hpp"

void git(const qpl::filesys::path& path, const state& state, history_status& history) {
	if (state.check_mode && !state.status) {
		return;
	}

	auto work_branch = path.branch_size() - 1;
	auto git_path = path.ensured_directory_backslash().with_branch(work_branch, "git");

	git_path.update();
	if (!git_path.exists()) {
		git_path = path;
	}

	qpl::filesys::path status_batch;
	qpl::filesys::path exec_batch;

	std::string status_data;
	std::string status_display_data;
	std::string exec_data;


	auto same_dir = qpl::filesys::get_current_location().string().front() == git_path.string().front();
	auto set_directory = qpl::to_string(same_dir ? "cd " : "cd /D ", git_path);

	auto home = qpl::filesys::get_current_location().ensured_directory_backslash();

	auto output_file = home.appended("output.txt");
	output_file.create();

	if (state.status) {
		if (state.action == action::pull || state.action == action::both) {
			status_batch = home.appended("git_status.bat");
			status_display_data = qpl::to_string("@echo off && ", set_directory, " && git fetch && git status -uno");
			status_data = qpl::to_string(status_display_data, " > ", output_file);
		}
		else if (state.action == action::push) {
			status_batch = home.appended("git_status.bat");
			status_display_data = qpl::to_string("@echo off && ", set_directory, " && git add -A && git status");
			status_data = qpl::to_string(status_display_data, " > ", output_file);
		}
	}
	else if (state.action == action::pull) {
		status_batch = home.appended("git_pull_status.bat");
		status_data = qpl::to_string("@echo off && ", set_directory, " && git fetch && git status -uno > ", output_file);

		exec_batch = home.appended("git_pull.bat");
		exec_data = qpl::to_string("@echo off && ", set_directory, " && @echo on && git pull");
	}
	else if (state.action == action::push) {
		status_batch = home.appended("git_push_status.bat");
		status_display_data = qpl::to_string("@echo off && ", set_directory, " && git add -A && git status");
		status_data = qpl::to_string(status_display_data, " > ", output_file);

		exec_batch = home.appended("git_push.bat");
		exec_data = qpl::to_string("@echo off && ", set_directory, " && git commit -m \"update\" && git push");
	}
	else if (state.hard_pull) {
		auto list = git_path.list_current_directory();
		for (auto& path : list) {
			if (path.get_full_name() != ".git") {
				path.remove();
			}
		}

		exec_batch = home.appended("git_reset.bat");
		exec_data = qpl::to_string("@echo off && ", set_directory, " && @echo on && git reset --hard HEAD && git clean -f -d");
		execute_batch(exec_batch, exec_data);
		return;
	}

	execute_batch(status_batch, status_data);

	auto lines = qpl::split_string(output_file.read(), '\n');
	if (lines.empty()) {
		qpl::println("error : no output from git status.");
		output_file.remove();
		return;
	}
	
	history.git_changes = true;
	if (state.action == action::pull) {
		if (lines.size() > 1) {
			std::string search1 = "Your branch is up to date";
			std::string search2 = "Your branch is ahead of";
			auto case1 = qpl::string_equals_ignore_case(lines[1].substr(0u, search1.length()), search1);
			auto case2 = qpl::string_equals_ignore_case(lines[1].substr(0u, search2.length()), search2);
			history.git_changes = !(case1 || case2);
		}
	}
	else if (state.action == action::push) {
		std::string search = "nothing to commit, working tree clean";
		history.git_changes = !qpl::string_equals_ignore_case(lines.back(), search);

		if (!history.git_changes && lines.size() > 1) {
			std::string search = "Your branch is ahead of";
			auto start = lines[1].substr(0u, search.length());
			history.git_changes = qpl::string_equals_ignore_case(start, search);
			
			if (!state.status && history.git_changes) {
				exec_batch = home.appended("git_push.bat");
				exec_data = qpl::to_string("@echo off && ", set_directory, " && git push");
			}
		}
	}


	if (history.git_changes) {
		if (!status_display_data.empty()) {
			if (state.print) {
				qpl::println();
				execute_batch(status_batch, status_display_data);
			}
		}
		if (!exec_data.empty()) {
			execute_batch(exec_batch, exec_data);
		}
	}
	output_file.remove();
}
