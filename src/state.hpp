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
			stream << "- " << command << "ing would cause local conflicts.\n";
		}
		if (this->local_changes) {
			stream << "- " << "needs to locally " << command << " files.\n";
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
	bool both_changes() const {
		return this->local_collision == false && this->local_changes == true && this->git_changes == true;
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
	bool check_mode = false;
	bool print = true;
	bool only_conflicts = false;
	bool find_collisions = true;
	bool status = false;
	bool quick_mode = false;
	bool update = false;
	::action action = action::both;
	::location location = location::both;
	std::vector<std::string> target_input_directories;

	void reset() {
		this->check_mode = false;
		this->print = true;
		this->only_conflicts = false;
		this->find_collisions = true;
		this->status = false;
		this->quick_mode = false;
		this->update = false;
		this->action = action::both;
		this->location = location::both;
		this->target_input_directories.clear();;

	}
};

struct history_status {
	bool move_changes = false;
	bool git_changes = false;
	bool any_output = false;

	std::unordered_set<std::string> checked;
	std::unordered_set<std::string> ignore;
	std::vector<std::string> data_overwrites;
	std::vector<std::string> time_overwrites;
	std::vector<std::string> removes;

	void reset() {
		this->move_changes = false;
		this->git_changes = false;
		this->data_overwrites.clear();
		this->time_overwrites.clear();
		this->removes.clear();
	}

	void command_reset() {
		this->any_output = false;
	}	
	bool any_serious_collisions() {
		return this->data_overwrites.size() || this->removes.size();
	}
	bool any_collisions() {
		return this->any_serious_collisions() || this->time_overwrites.size();
	}
	bool find_checked(std::string str) {
		return this->checked.find(str) != this->checked.cend();
	}
	bool find_ignored_root(qpl::filesys::path path) {
		for (auto& i : this->ignore) {
			if (path.has_root(i)) {
				return true;
			}
		}
		return false;
	}
	bool find_ignored(qpl::filesys::path path) {
		for (auto& i : this->ignore) {
			if (path == qpl::filesys::path(i)) {
				return true;
			}
		}
		return false;
	}
};
