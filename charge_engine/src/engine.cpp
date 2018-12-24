
#include "charge/charge.hpp"

#include "cache.hpp"
#include "compiler.hpp"
#include "config.hpp"
#include "process.hpp"
#include "tools.hpp"

#include <boost/algorithm/string/erase.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/max_element.hpp>
#include <boost/range/algorithm/transform.hpp>

#include <ctime>
#include <iterator>
#include <regex>

using namespace std::string_literals;

namespace charge
{

namespace
{

std::istream & get_line(std::istream & is, std::string & out)
{
    std::getline(is, out);
    boost::erase_all(out, "\r"); // To handle CRLF-style EOL
    return is;
}

std::string extract_import_trick(std::string const & chargetrick)
{
    std::istringstream is(chargetrick);
    std::string s;
    is >> s; // the comment
    is >> s; // chargetrick
    is >> s; // import
    is >> s; // library - keep this one
    return s;
}

StringList read_imported_libraries(std::istream & is)
{
    StringList libraries;

    std::regex re("//\\s+chargetrick\\s+import\\s.*");
    std::string line;
    while (get_line(is, line))
    {
        if (std::regex_match(line, re))
        {
            libraries.emplace_back(extract_import_trick(line));
        }
    }

    return libraries;
}

StringList library_part(StringList const & libraries,
    YAML::Node const & config, std::string const & part)
{
	if (libraries.empty()) return StringList();

	auto libraries_node = config["libraries"];
	if (!libraries_node)
	{
		// TODO: figure out/add exception
		// specific to missing "libraries" section in config.
		throw LibraryNotConfiguredError(libraries.front());
	}

	StringList r;

    for (auto lib : libraries)
    {
		auto node = libraries_node[lib];
        if (!node)
        {
            throw LibraryNotConfiguredError(lib);
        }

		auto s = node[part];

        if (s)
        {
            auto v = s.as<std::string>();
            r.push_back(v);
        }
    }

    return r;
}

StringList header_paths(StringList const & libraries, YAML::Node const & config)
{
    return library_part(libraries, config, "header_path");
}

StringList static_libraries(StringList const & libraries, YAML::Node const & config)
{
    return library_part(libraries, config, "static_library");
}

StringList system_libraries(StringList const & libraries, YAML::Node const & config)
{
	return library_part(libraries, config, "library");
}

StringList libpaths(StringList const & libraries, YAML::Node const & config)
{
	return library_part(libraries, config, "lib_path");
}

} // anonymous



LibraryNotConfiguredError::LibraryNotConfiguredError(std::string const & lib)
    : Exception("library "s + lib + " is not configured"),
    lib_(lib)
{}

std::string const & LibraryNotConfiguredError::library() const
{
    return lib_;
}


CommandLineArgumentError::CommandLineArgumentError(std::string const & msg)
	: Exception(msg)
{}


YAML::Node read_config(boost::filesystem::path const & fn)
{
    try
    {
        return YAML::LoadFile(fn.string());
    }
    catch (YAML::BadFile const &)
    {
        return YAML::Node();
    }
}


void write_config(YAML::Node const & config, 
		boost::filesystem::path const & fn)
{
	std::ofstream config_stream(fn.string());
	config_stream << config;
}


Libraries find_imports(YAML::Node const & config, std::istream & is)
{
    auto libraries = read_imported_libraries(is);

    Libraries libs;

    libs.header_paths_ = header_paths(libraries, config);
    libs.static_ = static_libraries(libraries, config);
    libs.system_ = system_libraries(libraries, config);
	libs.lib_paths_ = libpaths(libraries, config);

    return libs;
}


FileList decode_header_dependencies(std::string const & deps_text)
{
	std::istringstream is(deps_text);
	FileList retval;
	std::string str;
	while (std::getline(is, str))
	{
		retval.push_back(boost::filesystem::path{ str });
	}

	return retval;
}


std::string encode_header_dependencies(FileList const & deps)
{
	std::ostringstream os;

	boost::for_each(deps, 
		[&os](auto const & dep) {os << dep.string() << '\n'; }
	);

	return os.str();
}


std::time_t get_maybe_file_time(boost::filesystem::path const & path)
{
	using namespace boost::filesystem;

	if (exists(path))
	{
		return last_write_time(path);
	}

	return 0; // no file => first epoch second
}


int charge(boost::filesystem::path const & script, StringList const & args)
{
	//TODO: check if script exists.

	auto script_abspath{ boost::filesystem::absolute(script) };

	auto hostn{ hostname() };
	auto home{ home_path() };

	auto cache_path = get_cache_path(hostn, home, script_abspath);

	bool is_new_cache = create_cache(hostn, script_abspath, cache_path);

#if defined(CHARGE_WINDOWS)
	auto const exec_fn{ "executable.exe" };
#else
	auto const exec_fn{ "executable" };
#endif

	auto exec_path = cache_path / exec_fn;

    auto exec_time = get_maybe_file_time(exec_path);

	auto deps_file_contents = read_header_dependencies(cache_path);

	auto deps = decode_header_dependencies(deps_file_contents);

	std::vector<std::time_t> deps_time;

    boost::transform(
		deps,
		std::back_inserter(deps_time), 
		get_maybe_file_time);

	deps_time.push_back( get_maybe_file_time(script) );

	auto youngest_dep_time{ boost::max_element(deps_time) };

	// We are guaranteed to find one because list is never empty.
	assert(youngest_dep_time != deps_time.end());

	if ( !exec_time || std::difftime(*youngest_dep_time, exec_time) > 0 )
    {
        // Compile

		auto config_path{ home / ".charge" / "config" };
		auto config{ read_config(config_path) };
		
		std::ifstream script_stream(script.string());

		auto libraries( find_imports(config, script_stream) );

        YAML::Node compiler_config;

        if (config["compiler"])
		{
			compiler_config = config["compiler"];
		}
		else
		{
            compiler_config = configure();
            config["compiler"] = compiler_config;
            write_config(config, config_path);
        }

        Compiler compiler(compiler_config);

        Compiler::Arguments compiler_args;
        compiler_args.source_ = script;
        
		compiler_args.header_paths_ = libraries.header_paths_;
        compiler_args.static_libraries_ = libraries.static_;
		compiler_args.system_libraries_ = libraries.system_;
		compiler_args.lib_paths_ = libraries.lib_paths_;

        compiler_args.executable_output_fn_ = exec_path;

        FileList new_deps = compiler.compile(compiler_args);

        auto new_deps_file_contents = encode_header_dependencies(new_deps);

        write_header_dependencies(cache_path, new_deps_file_contents);
    }


	return exec(exec_path.string(), args);
}

} // charge



namespace std
{

std::ostream & operator << (std::ostream & os, charge::Libraries const & libs)
{
    os << "Libraries(\n";

	os << "  header paths: " << libs.header_paths_ << '\n';
    os << "  static: " << libs.static_ << '\n';
	os << "  system: " << libs.system_ << '\n';
	os << "  lib paths: " << libs.lib_paths_ << '\n';
    os << ")\n";
    return os;
}

std::ostream & operator << (std::ostream & os, charge::FileList const & fl)
{
	os << '(';
	if (fl.size())
	{
		auto set_it = fl.begin();
		os << *set_it;
		while (++set_it != fl.end())
		{
			os << ',' << *set_it;
		}
	}
	os << ')';
	return os;
}

} //std
