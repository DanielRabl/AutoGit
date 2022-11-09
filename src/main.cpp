#include <qpl/qpl.hpp>
#include <qpl/winsys.hpp>

#include "directory.hpp"

void execute(qpl::filesys::path path, const state& state, bool recursive = true) {
	if (path.string().starts_with("//")) {
		return;
	}
	path.ensure_directory_backslash();
	if (!path.exists()) {
		qpl::println('\\', path, "\\ doesn't exist.");
	}

	directory directory;
	directory.set_path(path);
	if (directory.empty() && recursive) {
		auto list = path.list_current_directory();
		for (auto& path : list) {
			execute(path, state, false);
		}
	}
	else {
		directory.execute(state);
	}
}

void execute(const std::vector<std::string>& lines, const state& state) {
	for (qpl::size i = 0u; i < lines.size(); ++i) {
		execute(qpl::string_remove_whitespace(lines[i]), state);
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
		state.mode.check_mode = false;
		state.mode.status = false;
		state.mode.quick_mode = false;
		state.mode.update = false;
		state.mode.location = location::both;
		state.mode.action = action::both;
		for (auto& arg : split) {
			if (qpl::string_equals_ignore_case(arg, "check")) {
				state.mode.check_mode = true;
			}
			else if (qpl::string_equals_ignore_case(arg, "quick")) {
				state.mode.quick_mode = true;
			}
			else if (qpl::string_equals_ignore_case(arg, "local")) {
				state.mode.location = location::local;
			}
			else if (qpl::string_equals_ignore_case(arg, "git")) {
				state.mode.location = location::git;
			}
			else if (qpl::string_equals_ignore_case(arg, "push")) {
				state.mode.action = action::push;
			}
			else if (qpl::string_equals_ignore_case(arg, "pull")) {
				state.mode.action = action::pull;
			}
			else if (qpl::string_equals_ignore_case(arg, "status")) {
				state.mode.status = true;
			}
			else if (qpl::string_equals_ignore_case(arg, "update")) {
				state.mode.update = true;
			}
			else {
				qpl::println("\"", arg, "\" invalid argument.\n");
				abort = true;
			}
		}
		if (abort) {
			continue;
		}
		if (state.mode.action == action::both && !(state.mode.status || state.mode.update)) {
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

		qpl::small_clock timer;

		state state;
		input_state(state);

		if (state.mode.action != action::both && !state.mode.status && !state.mode.update) {
			auto collision_state = state;

			collision_state.mode.find_collisions = true;
			collision_state.mode.print = false;
			collision_state.mode.check_mode = true;
			collision_state.mode.only_collisions = true;

			execute(location, collision_state);
			if (!confirm_collisions(collision_state)) {
				continue;
			}
		}
		execute(location, state);

		qpl::println_repeat('\n', 2);
		auto str = qpl::to_string("TOTAL : ", timer.elapsed().string_short());
		qpl::println_repeat("~", str.length());
		qpl::println(str);
		qpl::println_repeat("~", str.length());
		qpl::println_repeat('\n', 2);
	}
}

int main() try {
	run();
}
catch (std::exception& any) {
	qpl::println("caught exception:\n", any.what());
	qpl::system_pause();
}