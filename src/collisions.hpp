#pragma once

#include "state.hpp"
#include "info.hpp"

void print_collisions(const state& state) {
	auto action_word = state.action == action::pull ? "PULL" : "PUSH";
	auto size = info::time_overwrites.size();
	if (size) {
		qpl::println();
		qpl::println("HINT: There ", (size == 1 ? "is " : "are "), size, (size == 1 ? " file " : " files "), "where a ", qpl::color::aqua, action_word, " would overwrite a more recent version, but the data is same.");

		for (auto& i : info::time_overwrites) {
			qpl::println(qpl::color::light_aqua, ". . . . ", i);
		}
	}

	size = info::data_overwrites.size();
	if (size) {
		qpl::println();
		if (!state.check_mode) {
			qpl::print("WARNING: ");
		}
		qpl::println("There ", (size == 1 ? "is " : "are "), size, (size == 1 ? " file " : " files "), "where a ", qpl::color::light_red, action_word, " would overwrite a more recent version.");
		for (auto& i : info::data_overwrites) {
			qpl::println(qpl::color::light_red, ". . . . ", i);
		}
	}
	size = info::removes.size();
	if (size) {
		qpl::println();
		if (!state.check_mode) {
			qpl::print("WARNING: ");
		}
		qpl::println("There ", (size == 1 ? "is " : "are "), size, (size == 1 ? " file " : " files "), "where a ", qpl::color::light_red, action_word, " would remove them.");
		for (auto& i : info::removes) {
			qpl::println(qpl::color::light_red, ". . . . ", i);
		}
	}
}

bool confirm_collisions(const state& state) {
	if (info::total_change_sum && !state.status) {
		while (true) {
			qpl::println();
			auto word = info::total_change_sum > 1 ? "files" : "file";
			qpl::println("Are you SURE you want to overwrite ", qpl::color::light_red, info::total_change_sum, ' ', word, " ? (y / n)");
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
	return true;
}
