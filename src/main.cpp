#include <qpl/qpl.hpp>
#include <qpl/winsys.hpp>

#include "state.hpp"
#include "info.hpp"
#include "access.hpp"
#include "collisions.hpp"
#include "git.hpp"
#include "move.hpp"
#include "exe.hpp"

void execute(const std::string& line, qpl::time& time_sum, const state& state) {
	auto args = qpl::split_string_whitespace(line);

	if (args.empty()) {
		return;
	}
	if (args.size() == 1u) {
		qpl::println("\"", args.front(), "\" no command detected, ignored.");
		return;
	}
	auto path = args.back();
	auto dir_path = qpl::filesys::path(path).ensured_directory_backslash();

	if (args.front().starts_with("//")) {
		return;
	}
	if (args.size() == 2u && args[0] == "SOLUTION") {
		execute(qpl::to_string("MOVE GIT EXE ", args.back()), time_sum, state);
		return;
	}
	else if (args.size() == 2u && args[0] == "SOLUTIONS") {
		auto list = dir_path.ensured_directory_backslash().list_current_directory();
		for (auto& path : list) {
			auto valid = get_solution_directory_if_valid(path).has_value();
			if (valid) {
				execute(qpl::to_string("MOVE GIT EXE ", path), time_sum, state);
			}
		}
		return;
	}

	auto solution_optional = get_solution_directory_if_valid(dir_path);
	if (solution_optional.has_value()) {
		dir_path = solution_optional.value();
	}

	auto get_time_priority = [](std::string s) {
		if (s == "EXE") {
			return 0u;
		}
		else if (s == "MOVE") {
			return 1u;
		}
		else if (s == "GIT") {
			return 2u;
		}
		else {
			return 3u;
		}
	};

	if (state.action == action::pull) {
		std::sort(args.begin(), args.end() - 1, [&](auto a, auto b) {
			return get_time_priority(a) > get_time_priority(b);
			});
	}
	else if (state.action == action::push) {
		std::sort(args.begin(), args.end() - 1, [&](auto a, auto b) {
			return get_time_priority(a) < get_time_priority(b);
			});
	}

	auto active_command = [&](std::string command) {
		if (command == "MOVE" && state.location == location::git) {
			return false;
		}
		else if (command == "EXE" && (state.location == location::git || state.action == action::pull)) {
			return false;
		}
		else if (command == "GIT" && state.location == location::local) {
			return false;
		}
		else {
			return true;
		}
	};

	bool any_argument_active = false;
	for (qpl::size i = 0u; i < args.size() - 1; ++i) {
		const auto& command = args[i];
		if (active_command(command)) {
			any_argument_active = true;
			break;
		}
	}
	if (!any_argument_active) {
		return;
	}

	if (state.print) {
		qpl::println_repeat("\n", 2);
	}

	if (state.print) {
		for (qpl::size i = 0u; i < args.size() - 1; ++i) {
			const auto& command = args[i];
			qpl::color color = active_command(command) ? qpl::color::bright_white : qpl::color::gray;
			qpl::print(color, args[i], ' ');
		}
		qpl::println(qpl::color::aqua, dir_path);
	}

	info::reset();
	for (qpl::size i = 0u; i < args.size() - 1; ++i) {
		const auto& command = args[i];
		if (command == "MOVE") {
			if (state.location != location::git) {
				qpl::small_clock clock;
				if (state.status) {
					auto status_state = state;
					status_state.check_mode = true;
					move(dir_path, status_state);
				}
				else {
					move(dir_path, state);
				}
				time_sum += clock.elapsed();
			}
		}
		else if (command == "GIT") {
			if (state.location != location::local) {
				qpl::small_clock clock;
				git(dir_path, state);
				time_sum += clock.elapsed();
			}
		}
		else if (command == "EXE") {
			if (state.location != location::git) {
				qpl::small_clock clock;
				exe(dir_path, state);
				time_sum += clock.elapsed();
			}
		}
		else if (command == "IGNORE") {
			info::ignore.insert(dir_path);
		}
		else {
			if (state.print) {
				qpl::println("unkown command \"", command, "\" ignored.");
			}
		}
	}
	if (state.print || state.find_collisions) {
		print_collisions(state);
	}
}

void execute(const std::vector<std::string>& lines, qpl::time& time_sum, const state& state) {
	for (qpl::size i = 0u; i < lines.size(); ++i) {
		execute(lines[i], time_sum, state);
	}
}

void input_state(state& state) {
	while (true) {
		qpl::print("PULL changes or PUSH changes > ");
		auto input = qpl::get_input();

		auto split = qpl::split_string_whitespace(input);
		if (split.empty()) {
			continue;
		}

		bool abort = false;
		state.check_mode = false;
		state.status = false;
		state.quick_mode = false;
		state.location = location::both;
		state.action = action::none;
		for (auto& arg : split) {
			if (qpl::string_equals_ignore_case(arg, "check")) {
				state.check_mode = true;
			}
			else if (qpl::string_equals_ignore_case(arg, "quick")) {
				state.quick_mode = true;
			}
			else if (qpl::string_equals_ignore_case(arg, "local")) {
				state.location = location::local;
			}
			else if (qpl::string_equals_ignore_case(arg, "git")) {
				state.location = location::git;
			}
			else if (qpl::string_equals_ignore_case(arg, "push")) {
				state.action = action::push;
			}
			else if (qpl::string_equals_ignore_case(arg, "pull")) {
				state.action = action::pull;
			}
			else if (qpl::string_equals_ignore_case(arg, "status")) {
				state.status = true;
			}
			else {
				qpl::println("\"", arg, "\" invalid argument.\n");
				abort = true;
			}
		}
		if (abort) {
			continue;
		}
		if (state.action == action::none && !state.status) {
			qpl::println("\"", split, "\" invalid arguments.\n");
			continue;
		}
		return;
	}
}

std::vector<std::string> find_location() {
	auto lines = qpl::split_string(qpl::read_file("paths.cfg"), '\n');

	for (auto& line : lines) {
		qpl::remove_multiples(line, '\r');
	}

	std::vector<std::vector<std::string>> locations;
	std::vector<std::vector<std::string>> locations_command;
	std::vector<std::string> location_names;
	for (qpl::size i = 0u; i < lines.size(); ++i) {
		if (lines[i].empty()) {
			continue;
		}
		if (lines[i].back() == '{') {
			locations.push_back({});
			locations_command.push_back({});
			location_names.push_back(qpl::split_string_words(lines[i]).front());
		}
		else if (lines[i].front() != '}') {
			auto split = qpl::split_string_whitespace(lines[i]);
			locations.back().push_back(split.back());
			locations_command.back().push_back(lines[i]);
		}
	}

	qpl::size best_index = 0u;
	std::vector<std::string> location;
	std::vector<std::string> result;
	for (qpl::size i = 0u; i < locations.size(); ++i) {
		bool found = true;
		for (auto& p : locations[i]) {
			if (!qpl::filesys::exists(p)) {
				found = false;
				break;
			}
		}
		if (found) {
			location = locations[i];
			result = locations_command[i];
			best_index = i;
			break;
		}
	}
	if (location.empty()) {
		std::vector<std::pair<qpl::size, qpl::size>> counts(locations.size());

		for (qpl::size i = 0u; i < locations.size(); ++i) {
			counts[i].first = i;
			counts[i].second = 0u;
			for (auto& p : locations[i]) {
				if (qpl::filesys::exists(p)) {
					++counts[i].second;
				}
			}
		}

		qpl::sort(counts, [](auto a, auto b) {
			return a.second > b.second;
			});

		best_index = counts.front().first;
		qpl::println("paths.cfg : couldn't find the right location.\nBest match is location \"", location_names[best_index], "\": ");
		for (auto& i : locations[best_index]) {
			if (qpl::filesys::exists(i)) {
				qpl::set_console_color(qpl::foreground::light_green);
				qpl::print("FOUND     ");
			}
			else {
				qpl::set_console_color(qpl::foreground::light_red);
				qpl::print("NOT FOUND ");
			}
			qpl::set_console_color_default();
			qpl::println(" -- ", i, " ]");
		}
		qpl::system_pause();
	}
	else {
		qpl::println(qpl::color::gray, qpl::to_string("location = \"", location_names[best_index], "\"\n"));
	}

	return result;
}

void run() {
	auto location = find_location();
	if (location.empty()) {
		return;
	}

	while (true) {
		info::total_reset();

		qpl::time time_sum = 0u;

		state state;
		input_state(state);

		if (state.status) {
			state.print = true;
			state.check_mode = true;
			
			bool status_push = (state.action == action::push) || (state.action == action::none);
			bool status_pull = (state.action == action::pull) || (state.action == action::none);

			auto location_string = state.location == location::git ? "GIT" : state.location == location::local ? "LOCAL" : "GIT && LOCAL";

			auto print = [&](std::string command) {
				qpl::println(qpl::color::light_blue, qpl::to_string_repeat("= ", 25));
				auto str = qpl::to_string("\tSTATUS CHECK - ", location_string, ' ', command);
				qpl::println(qpl::color::light_blue, str);
				qpl::println(qpl::color::light_blue, qpl::to_string_repeat("= ", 25));
			};

			if (status_push) {
				qpl::println_repeat("\n", 2);
				print("PUSH");
				state.action = action::push;
				execute(location, time_sum, state);

				if (state.location != location::git) {
					verify_collisions(state);
				}
			}
			if (status_pull) {
				if (status_push && state.print) {
					qpl::println_repeat("\n", 2);
				}
				print("PULL");
				info::total_reset();

				state.action = action::pull;
				execute(location, time_sum, state);

				if (state.location != location::git) {
					verify_collisions(state);
				}
			}
		}
		else {
			state.print = false;
			state.find_collisions = true;

			auto check_mode = state.check_mode;
			state.check_mode = true;

			execute(location, time_sum, state);
			if (verify_collisions(state)) {
				state.check_mode = check_mode;
				state.print = true;
				state.find_collisions = false;
				execute(location, time_sum, state);
			}
		}
		
		qpl::println();
		qpl::println();
		auto str = qpl::to_string("TOTAL : ", time_sum.string_until_ms());
		qpl::println_repeat("~", str.length());
		qpl::println(str);
		qpl::println_repeat("~", str.length());
		qpl::println_repeat("\n", 3);
	}
}

int main() try {
	run();
}
catch (std::exception& any) {
	qpl::println("caught exception:\n", any.what());
	qpl::system_pause();
}