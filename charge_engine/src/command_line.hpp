#ifndef GUARD_f06d2694546e4dc58f151f6c02969964
#define GUARD_f06d2694546e4dc58f151f6c02969964

#include <charge/types.hpp>

#include <iostream>

namespace charge
{

class Commandable;

int run_command_line(StringList const & args, Commandable & cmds, std::ostream & user_console);

} // charge

#endif
