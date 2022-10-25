#pragma once

enum class action {
	push,
	pull,
	none
};

enum class location {
	local,
	git,
	both
};
struct state {
	bool check_mode = true;
	bool print = true;
	bool find_collisions = true;
	action action = action::none;
	::location location = location::both;
	bool status = false;
	bool quick_mode = false;
};
