
#include "../src/InclusionNotesSniffer.hpp"

#include <boost/test/unit_test.hpp>

#include <deque>


BOOST_AUTO_TEST_SUITE(InclusionNotesSniffer);

namespace
{

class MockStream : public charge::ReadableStream
{
public:
	explicit MockStream(std::deque<std::string> const & sl)
		: input_(sl)
	{}

	virtual boost::optional<std::string> read()
	{
		if (input_.empty()) return boost::optional<std::string>();

		auto retval = input_.front();
		input_.pop_front();
		return retval;
	}

	std::deque<std::string> input_;

};

}


BOOST_AUTO_TEST_CASE(empty_input)
{
	MockStream mock = MockStream(std::deque<std::string>());

	charge::InclusionNotesSniffer sniffer(mock);

	BOOST_CHECK(!sniffer.read());

	BOOST_CHECK_EQUAL(sniffer.inclusions_.size(), 0);
}


BOOST_AUTO_TEST_CASE(just_warnings)
{
	MockStream mock = MockStream(
		{
			"warning: ",
			"such and such\n",
		}
	);

	charge::InclusionNotesSniffer sniffer(mock);

	auto line = sniffer.read();

	BOOST_CHECK(line);
	BOOST_CHECK_EQUAL(*line, "warning: such and such\n");


	line = sniffer.read();
	
	BOOST_CHECK(!line);

	BOOST_CHECK_EQUAL(sniffer.inclusions_.size(), 0);
}


BOOST_AUTO_TEST_CASE(one_note)
{
	MockStream mock = MockStream(
		{
			"pgm.cpp\n",
			"Note: including file:   stdcpp/iostream\n",
			"warning: ",
			"such and such\n",
		}
	);

	charge::InclusionNotesSniffer sniffer(mock);

	auto line = sniffer.read();

	BOOST_CHECK(line);
	BOOST_CHECK_EQUAL(*line, "pgm.cpp\n");

	
	line = sniffer.read();
	
	BOOST_CHECK(line);
	BOOST_CHECK_EQUAL(*line, "warning: such and such\n");

	
	line = sniffer.read();
	
	BOOST_CHECK(!line);

	BOOST_REQUIRE_EQUAL(sniffer.inclusions_.size(), 1);

	BOOST_CHECK_EQUAL(sniffer.inclusions_[0], "stdcpp/iostream");
}


BOOST_AUTO_TEST_CASE(no_final_lf)
{
	MockStream mock = MockStream(
		{
			"warning: ",
			"such and such"
		}
	);

	charge::InclusionNotesSniffer sniffer(mock);

	auto line = sniffer.read();

	BOOST_CHECK(line);
	BOOST_CHECK_EQUAL(*line, "warning: such and such");


	line = sniffer.read();
	
	BOOST_CHECK(!line);

	BOOST_CHECK_EQUAL(sniffer.inclusions_.size(), 0);
}


BOOST_AUTO_TEST_SUITE_END();
