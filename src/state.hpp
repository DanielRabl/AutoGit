#pragma once

enum class action {
	push,
	pull,
	both
};
enum class location {
	local,
	git,
	both
};
enum class command {
	exe = 0,
	move,
	git
};
struct status {
	bool local_collision = false;
	bool local_changes = false;
	bool git_changes = false;
	bool time_overwrites = false;

	void reset() {
		this->local_collision = false;
		this->local_changes = false;
		this->git_changes = false;
		this->time_overwrites = false;
	}
	std::string string(std::string command) const {
		std::ostringstream stream;

		if (this->local_collision) {
			stream << "- " << command << "ing would cause conflicts.\n";
		}
		if (this->local_changes) {
			stream << "- " << "needs to " << command << " local files.\n";
		}
		if (this->git_changes) {
			stream << "- " << "needs to GIT " << command << "\n";
		}
		return stream.str();
	}
	bool clean() const {
		return this->local_collision == false && this->local_changes == false && this->git_changes == false;
	}
	bool just_git_changes() const {
		return this->local_collision == false && this->local_changes == false && this->git_changes == true;
	}
	bool can_execute() const {
		return !this->local_collision && ((this->local_changes && !this->git_changes) || this->just_git_changes());
	}
};

constexpr auto command_string(command command) {
	switch (command) {
	case command::exe: return "EXE";
	case command::move: return "MOVE";
	case command::git: return "GIT";
	}
	return "";
}

struct state {
	bool check_mode = true;
	bool print = true;
	bool only_collisions = false;
	bool find_collisions = true;
	::action action = action::both;
	::location location = location::both;
	bool status = false;
	bool quick_mode = false;
	bool update = false;
};
