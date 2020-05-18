// http://cppunit.sourceforge.net/doc/cvs/cppunit_cookbook.html
#include <iostream>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TextTestRunner.h>
#include <cppunit/TestResult.h>
#include <cppunit/BriefTestProgressListener.h>

int main(int argc, char **argv)
{
	CppUnit::TextTestRunner runner;
	CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry();
	CppUnit::BriefTestProgressListener listener;
	runner.eventManager().addListener(&listener);
	runner.addTest(registry.makeTest());
	bool run_ok = runner.run("", false,true,false);
	return !run_ok;
}

