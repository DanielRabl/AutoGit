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
		else if (qpl::string_equals_ignore_case(arg, "hard-pull")) {
			state.hard_pull = true;
		}
		else {
			if (arg.length() > 1 && arg.starts_with('"') && arg.back() == '"') {
				arg = arg.substr(1u, arg.length() - 2u);
			}
			std::vector<std::string> list;
			for (auto& i : autogit.directories) {
				if (i.directory_name.length() >= arg.length()) {
					list.push_back(i.directory_name.substr(0u, arg.length()));
				}
				else {
					list.push_back("");
				}
			}
			auto best_match_indices = qpl::best_string_matches_indices(list, arg);

			bool started_match = false;
			for (auto& i : list) {
				if (qpl::string_starts_with_ignore_case(i, arg)) {
					started_match = true;
					break;
				}
			}

			qpl::filesys::path target;

			if (!started_match) {
				qpl::println("couldn't find a directory named \"", arg, "\".");

				if (best_match_indices.size() > 1) {
					while (true) {
						qpl::println("select the right location: [enter to go back] \n");
						for (qpl::size i = 0u; i < best_match_indices.size(); ++i) {
							qpl::println(qpl::color::aqua, qpl::to_string('<', i, '>'), " ", autogit.directories[best_match_indices[i]].path);
						}
						qpl::print("> ");
						auto number = qpl::get_input();
						if (qpl::is_string_number(number)) {
							auto index = qpl::size_cast(number);
							if (index < best_match_indices.size()) {
								target = autogit.directories[best_match_indices[index]].path;
								break;
							}
						}
						abort = true;
						break;
					}
				}
				else {
					while (true) {
						qpl::print("did you mean this location ", qpl::color::aqua, autogit.directories[best_match_indices.front()].path, "? (y / n) > ");
						auto input = qpl::get_input();
						if (qpl::string_equals_ignore_case(input, "y")) {
							target = autogit.directories[best_match_indices.front()].path;
							break;
						}
						else if (qpl::string_equals_ignore_case(input, "n")) {
							abort = true;
							break;
						}
						qpl::println("invalid input \"", input, "\".\n");
					}
				}
			}
			else {
				target = autogit.directories[best_match_indices.front()].path;
				if (best_match_indices.size() > 1) {
					while (true) {
						qpl::println("there are multiple directories that match \"", arg, "\".");
						qpl::println("did you mean any of these locations? [enter to go back] ");
						for (qpl::size i = 0u; i < best_match_indices.size(); ++i) {
							qpl::println(qpl::color::aqua, qpl::to_string('<', i, '>'), " ", autogit.directories[best_match_indices[i]].path);
						}
						qpl::print("> ");
						auto number = qpl::get_input();
						if (qpl::is_string_number(number)) {
							auto index = qpl::size_cast(number);
							if (index < best_match_indices.size()) {
								target = autogit.directories[best_match_indices[index]].path;
								break;
							}
						}
						abort = true;
						break;
					}
				}
			}
			if (!target.empty()) {
				qpl::println("selected ", qpl::color::aqua, target);
				state.target_input_directories.push_back(target);
			}
		}
	}
	if (abort) {
		return false;
	}
	if (state.action == action::both && !(state.status || state.update || state.hard_pull)) {
		qpl::println("\"", split, "\" invalid arguments.\n");
		return false;
	}
	return true;
}

void print_commands() {
	auto ps = qpl::color::light_aqua;
	auto pl = qpl::color::light_green;
	auto u = qpl::color::aqua;

	auto seperation_width = 20;
	qpl::println();
	qpl::println(qpl::color::aqua, "git pull . . . ", ">> ", pl, "fetches", " and ", pl, "merges", " from git.");
	qpl::println(qpl::color::gray, qpl::to_string_repeat("- ", seperation_width));
	qpl::println(qpl::color::aqua, "git push . . . ", ">> ", ps, "commits" , " and ", ps, "pushes",  " to git.");
	qpl::println(qpl::color::gray, qpl::to_string_repeat("- ", seperation_width));
	qpl::println(qpl::color::aqua, "local pull . . ", ">> ", "git directory -> solution directory.");
	qpl::println(qpl::color::gray, qpl::to_string_repeat("- ", seperation_width));
	qpl::println(qpl::color::aqua, "local push . . ", ">> ", "solution directory -> git directory.");
	qpl::println(qpl::color::gray, qpl::to_string_repeat("- ", seperation_width));
	qpl::println(qpl::color::aqua, "pull . . . . . ", ">> ", "BOTH git ", pl, "pull", " && local ", pl, "pull", ".");
	qpl::println(qpl::color::gray, qpl::to_string_repeat("- ", seperation_width));
	qpl::println(qpl::color::aqua, "push . . . . . ", ">> ", "BOTH local ", ps, "push", " && git ", ps, "push", ".");
	qpl::println(qpl::color::gray, qpl::to_string_repeat("- ", seperation_width));
	qpl::println(qpl::color::aqua, "status . . . . ", ">> ", "shows ", u, "status", " of any command.");
	qpl::println(qpl::color::gray, qpl::to_string_repeat("- ", seperation_width));
	qpl::println(qpl::color::aqua, "update . . . . ", ">> ", "runs ", u, "status", " and executes either ", ps, "push", " or ", pl, "pull", ".");
	qpl::println(qpl::color::gray, qpl::to_string_repeat("- ", seperation_width));
	qpl::println(qpl::color::aqua, "hard-pull  . . ", ">> ", "hard resets git and runs ", pl, "pull", ".");
	qpl::println(qpl::color::gray, qpl::to_string_repeat("- ", seperation_width));
	qpl::println(qpl::color::aqua, "[directory]. . ", ">> ", "runs any command ONLY on that directory.");
	qpl::println();
	qpl::println("combine them, e.g. \"local status\", \"git pull\", \"local push status\".\n");
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
	std::vector<std::string> location_names;
	for (qpl::size i = 0u; i < lines.size(); ++i) {
		if (lines[i].empty()) {
			continue;
		}
		if (lines[i].back() == '{') {
			locations.push_back({});
			location_names.push_back(qpl::split_string_words(lines[i]).front());
		}
		else if (lines[i].front() != '}') {
			qpl::size index = 0u;
			while (index < lines[i].size() && qpl::is_character_whitespace(lines[i][index])) {
				++index;
			}
			locations.back().push_back(lines[i].substr(index));
		}
	}

	qpl::size best_index = 0u;
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
			result = locations[i];
			best_index = i;
			break;
		}
	}
	if (result.empty()) {
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
		qpl::println(qpl::color::gray, qpl::to_string("location = \"", location_names[best_index], "\""));
	}

	return result;
}

void run_loop(autogit& autogit) {

	while (true) {
		info::total_reset();

		state state;
		input_state(state, autogit);

		autogit.execute(state);
	}
}

void run(const std::vector<std::string>& input, autogit& autogit) {
	for (auto& input : input) {
		info::total_reset();

		state state;
		input_state(state, input, autogit);

		autogit.execute(state);
	}
	run_loop(autogit);
}

int main(int argc, char** argv) try {
	print_commands();
	auto location = find_location();
	if (location.empty()) {
		qpl::println("no directories found.");
		qpl::system_pause();
		return 0;
	}

	autogit autogit;
	autogit.find_directories(location);
	autogit.print();
	qpl::println();

	if (argc > 1) {
		std::vector<std::string> args(argc - 1);
		for (qpl::isize i = 0; i < argc - 1; ++i) {
			args[i] = argv[i + 1];
		}
		run(args, autogit);
	}
	else {
		run_loop(autogit);
	}
}
catch (std::exception& any) {
	qpl::println("caught exception:\n", any.what());
	qpl::system_pause();
}