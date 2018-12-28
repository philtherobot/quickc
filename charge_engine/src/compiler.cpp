#include "compiler.hpp"

#include "InclusionNotesSniffer.hpp"
#include "process.hpp"
#include "tools.hpp"

#include <charge/exception.hpp>

#include <iostream>

#include <boost/algorithm/string/join.hpp>


using namespace std::string_literals;


namespace charge
{


namespace
{


class FirstLineFilter : public ReadableStream
{
public:
	explicit FirstLineFilter(ReadableStream & source);

	virtual boost::optional<std::string> read();

private:
	ReadableStream & source_;
	std::string accumulator_;
	bool first_line_skipped_;
};


FirstLineFilter::FirstLineFilter(ReadableStream & source)
: source_(source), first_line_skipped_(false)
{}


boost::optional<std::string> FirstLineFilter::read()
{
	for (;;)
	{
		auto new_block = source_.read();

		if (first_line_skipped_)
		{
			return new_block;
		}

		accumulator_ += *new_block;

		auto line = consume_line(accumulator_);

		if (line)
		{
			first_line_skipped_ = true;

			if (!accumulator_.empty())
			{
				std::string output;
				swap(output, accumulator_);
				return output;
			}
		}
	}
}


} // anonymous


Config configure()
{
    // TODO
    Config conf;

    conf["compiler"] = "/usr/bin/g++";
    conf["version"] = "7.2.0";
    conf["family"] = "g++";

    return conf;
}


Compiler::Compiler(Config const & conf)
    : config_(conf)
{}


FileList Compiler::compile(Arguments const & args)
{
    auto family = config_["family"].as<std::string>();

	std::string cmd;

    if (family == "msvc")
    {
		return compile_with_msvc(args);
    }
    else
    {
        throw UnsupportedFamilyError(family);
    }

	ShellProcess proc;

	proc.start(cmd);

	while (auto res = proc.child_stdout_->read())
	{
		std::cout << *res;
	}

	return FileList();
}


std::string Compiler::msvc_command_line(Arguments const & args) const
{
	auto s = args.executable_output_fn_.stem(); // "fn.exe" -> "fn"
	s += ".obj";
	auto p = args.executable_output_fn_.parent_path() / s;

	auto cl_cmd = config_["command"].as<std::string>();

	StringList options{
		quote_if_needed(cl_cmd),
		"/nologo",
		"/TP",
		"/MD",
		"/showIncludes",
		"/EHsc",
		quote_if_needed("/Fe:" + args.executable_output_fn_.string()),
		quote_if_needed("/Fo:" + p.string())
	};


	for (auto header_path : args.header_paths_)
	{
		options.push_back( quote_if_needed("/I"s + header_path) );
	}


	StringList linker_options;

	for (auto lib_path : args.lib_paths_)
	{
		linker_options.push_back(quote_if_needed("/LIBPATH:"s + lib_path));
	}


	StringList words;

	words.insert(words.end(), options.begin(), options.end());
	words.insert(words.end(), quote_if_needed(args.source_.string()));

	if (!linker_options.empty())
	{
		words.insert(words.end(), "/link"s);
		words.insert(words.end(), linker_options.begin(), linker_options.end());
	}

	return boost::algorithm::join(words, " ");
}


FileList Compiler::compile_with_msvc(Arguments const & args) const
{
	auto cmd = msvc_command_line(args);

	ShellProcess proc;

	proc.start(cmd);

	InclusionNotesSniffer sniffer( *proc.child_stdout_ );

	FirstLineFilter filter(sniffer);

	while (auto res = filter.read())
	{
		std::cout << *res;
	}

	return sniffer.inclusions_;
}


} // charge

