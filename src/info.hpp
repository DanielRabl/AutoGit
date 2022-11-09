#pragma once

#include <qpl/qpl.hpp>
#include "state.hpp"

namespace info {
	qpl::size total_change_sum = 0u;
	constexpr auto print_space = 40;

	void total_reset() {
		total_change_sum = 0u;
	}
}

constexpr bool print_ignore = false;