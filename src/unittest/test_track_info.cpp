#include <cppunit/extensions/HelperMacros.h>
#include <exception>
#include <cstdio>
#include "../track_info.h"
#include "song.h"
#include "input.h"
#include "mml_input.h"

class Track_Info_Test : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(Track_Info_Test);
	CPPUNIT_TEST(test_generator);
	CPPUNIT_TEST_SUITE_END();
private:
	Song *song;
	MML_Input *mml_input;
public:
	void setUp()
	{
		song = new Song();
		mml_input = new MML_Input(song);
	}
	void tearDown()
	{
		delete mml_input;
		delete song;
	}
	// result should be the same
	void test_generator()
	{
		mml_input->read_line("A c1 r2 c2 @2 v12 ^4 &c8");
		Track_Info info = Track_Info_Generator(*song, song->get_track(0));
		auto& track_map = info.events;
		auto it = track_map.begin();
		CPPUNIT_ASSERT_EQUAL((int)0, it->first);
		CPPUNIT_ASSERT_EQUAL((uint16_t)96, it->second.on_time);
		it++;
		CPPUNIT_ASSERT_EQUAL((int)96, it->first);
		CPPUNIT_ASSERT_EQUAL((uint16_t)48, it->second.off_time);
		it++;
		CPPUNIT_ASSERT_EQUAL((int)144, it->first);
		CPPUNIT_ASSERT_EQUAL((uint16_t)48, it->second.on_time);
		it++;
		// the two volume/instrument commands should not be in the list
		CPPUNIT_ASSERT_EQUAL((int)192, it->first);
		CPPUNIT_ASSERT_EQUAL(true, it->second.is_tie);
		CPPUNIT_ASSERT_EQUAL((uint16_t)24, it->second.on_time);
		it++;
		CPPUNIT_ASSERT_EQUAL((int)216, it->first);
		CPPUNIT_ASSERT_EQUAL(true, it->second.is_slur);
		CPPUNIT_ASSERT_EQUAL((uint16_t)12, it->second.on_time);
		it++;
		CPPUNIT_ASSERT(it == track_map.end());
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(Track_Info_Test);

