#include <qpl/qpl.hpp>

#include "autogit.hpp"


bool input_state(state& state, const std::string& input, const autogit& autogit) {
	auto split = qpl::split_string_whitespace(input);
	if (split.empty()) {
		return false;
	}

	bool abort = false;
	state.reset();

	for (auto& arg : split) {

		std::vector<qpl::filesys::path> found_locations;
		for (auto& dir : autogit.directories) {
			auto project_name = dir.path.ensured_directory_backslash().get_directory_name();
			if (arg.length() > 1 && arg.starts_with('"') && arg.back() == '"') {
				arg = arg.substr(1u, arg.length() - 2u);
			}
			if (qpl::string_equals_ignore_case(project_name, arg)) {
				found_locations.push_back(dir.path);
			}
		}

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
		else if (qpl::string_equals_ignore_case(arg, "update")) {
			state.update = true;
		}
		else if (found_locations.size()) {
			qpl::filesys::path target = found_locations.front();
			if (found_locations.size() > 1) {
				while (true) {
					qpl::println("there are multiple directories that match \"", arg, "\".");
					qpl::println("select the right location: \n");
					for (qpl::size i = 0u; i < found_locations.size(); ++i) {
						qpl::println(qpl::color::aqua, qpl::to_string('<', i, '>'), " ", found_locations[i]);
					}
					qpl::print("> ");
					auto number = qpl::get_input();
					if (qpl::is_string_number(number)) {
						auto index = qpl::size_cast(number);
						if (index < found_locations.size()) {
							target = found_locations[index];
							qpl::println("selected ", target);
							break;
						}
					}
					qpl::println("invalid input.\n");
				}
			}
			state.target_input_directories.push_back(target);
		}
		else {
			qpl::println("\"", arg, "\" invalid argument.\n");
			abort = true;
		}
	}
	if (abort) {
		return false;
	}
	if (state.action == action::both && !(state.status || state.update)) {
		qpl::println("\"", split, "\" invalid arguments.\n");
		return false;
	}
	return true;
}
void print_commands() {
	auto ps = qpl::color::light_aqua;
	auto pl = qpl::color::light_green;
	auto u = qpl::color::aqua;
	qpl::println(qpl::color::aqua, "git pull", " . . . ", pl, "fetches", " and ", pl, "merges", " from git.");
	qpl::println(qpl::color::aqua, "git push", " . . . ", ps, "commits" , " and ", ps, "pushes",  " to git.");
	qpl::println(qpl::color::aqua, "local pull", " . . git directory -> solution directory.");
	qpl::println(qpl::color::aqua, "local push", " . . solution directory -> git directory.");
	qpl::println(qpl::color::aqua, "pull", " . . . . . BOTH git ", pl, "pull", " && local ", pl, "pull", ".");
	qpl::println(qpl::color::aqua, "push", " . . . . . BOTH local ", ps, "push", " && git ", ps, "push", ".");
	qpl::println(qpl::color::aqua, "status", " . . . . shows ", u, "status", " of any command.");
	qpl::println(qpl::color::aqua, "update", " . . . . runs ", u, "status", " and executes either ", ps, "push", " or ", pl, "pull", ".");
	qpl::println(qpl::color::aqua, "check", "  . . . . runs any command, but in safe mode.");
	qpl::println(qpl::color::aqua, "quick", "  . . . . runs any command, but less deep.");
	qpl::println(qpl::color::aqua, "[directory]\"", ". . runs any command ONLY on that directory.");
	qpl::println();
	qpl::println("combine them, e.g. \"local status\", \"git pull\", \"local push status\".");
}

void input_state(state& state, const autogit& autogit) {
	while (true) {

		qpl::print("command > ");
		auto input = qpl::get_input();

		if (input_state(state, input, autogit)) {
			return;
		}
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

			for (auto& i : result) {
				qpl::size index = 0u;
				while (index < i.length() && qpl::is_character_whitespace(i[index])) {
					++index;
				}
				if (index < i.length()) {
					i = i.substr(index);
				}
			}

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

void run_loop() {
	auto location = find_location();
	if (location.empty()) {
		return;
	}

	autogit autogit;

	while (true) {
		info::total_reset();

		autogit.find_directories(location);

		state state;
		input_state(state, autogit);

		autogit.execute(state);
	}
}

void run(const std::vector<std::string>& input) {
	auto location = find_location();
	if (location.empty()) {
		return;
	}

	autogit autogit;
	for (auto& input : input) {
		info::total_reset();

		autogit.find_directories(location);

		state state;
		input_state(state, input, autogit);

		autogit.execute(state);
	}
	run_loop();
}

int main(int argc, char** argv) try {
	print_commands();

	if (argc > 1) {
		std::vector<std::string> args(argc - 1);
		for (qpl::isize i = 0; i < argc - 1; ++i) {
			args[i] = argv[i + 1];
		}
		run(args);
	}
	else {
		run_loop();
	}
}
catch (std::exception& any) {
	qpl::println("caught exception:\n", any.what());
	qpl::system_pause();
}