#ifndef GUARD_c32a67b8d56e4694b3957351dd917844
#define GUARD_c32a67b8d56e4694b3957351dd917844

#include <boost/filesystem.hpp>

namespace charge
{


boost::filesystem::path get_cache_path(
	std::string const & hostname,
	boost::filesystem::path const & home,
	boost::filesystem::path const & script_abspath);


bool create_cache(
	std::string const & hostname,
	boost::filesystem::path const & script_abspath,
	boost::filesystem::path const & cache_abspath);


std::string read_dependencies(boost::filesystem::path const & cache_abspath);

void write_dependencies(boost::filesystem::path const & cache_abspath,
	std::string const & deps_text);


} // charge

#endif