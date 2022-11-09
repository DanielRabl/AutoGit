#pragma once

#include <qpl/qpl.hpp>
#include <qpl/winsys.hpp>


void execute_batch(const std::string& path, const std::string& data) {
	qpl::filesys::create_file(path, data);
	std::system(path.c_str());
	qpl::filesys::remove(path);
}
