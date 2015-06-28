/*
	Project FreeAX25
    Copyright (C) 2015  tania@df9ry.de

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if 0
#include "XMLRuntime.h"
#include "Environment.h"
#include "Configuration.h"
#include "StringUtil.h"
#include "TimerManager.h"

#include <xercesc/dom/DOM.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/util/PlatformUtils.hpp>

#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace std;
using namespace FreeAX25.Runtime;
#include <StringUtil.h>

#endif

#include <Environment.h>
#include <TimerManager.h>
#include <LoadableObject.h>

#include <string>
#include <cstdlib>
#include <memory>

#ifdef _WIN32
# include <direct.h>
#else
# include <unistd.h>
#endif

using namespace std;
using namespace FreeAX25::Runtime;

int main(int argc, const char* argv[]) {
	unique_ptr<Environment> environment{new Environment()};
	env(environment.get());
	env().logDebug("Start program");

	// Change to program path to get shared objects loaded:
	string startdir{argv[0]};
#ifdef _WIN32
	size_t pos = startdir.find_last_of("/\\");
	if (pos != string::npos) {
		startdir.erase(pos);
		env().logDebug("Goto WIN32 directory: \"" + startdir);
		_chdir(startdir.c_str());
	}
#else
	size_t pos = startdir.find_last_of('/');
	if (pos != string::npos) {
		startdir.erase(pos);
		env().logDebug("Goto posix directory: \"" + startdir + "\"");
		chdir(startdir.c_str());
	}
#endif

	// Get the XML file path. This is either specified as an argument
	// argv[1] or can be deduced by argv[0]:
	string xmlpath{};
	if (argc > 1) {
		xmlpath.append(argv[1]);
	} else {
		xmlpath.append(argv[0]);
#ifdef _WIN32
		pos = xmlpath.find_last_of("/\\");
		if (pos != string::npos) xmlpath.erase(0, pos + 1);
		if (StringUtil::endsWith(StringUtil::toLower(xmlpath), ".exe"))
			xmlpath = xmlpath.substr(0, xmlpath.size()-4);
#else
		pos = xmlpath.find_last_of('/');
		if (pos != string::npos) xmlpath.erase(0, pos + 1);
#endif
		xmlpath.append(".xml");
	}
	// Load configuration:
	try {
		env().logInfo("Parse " + xmlpath);
		LoadableObject lo{};
		unique_ptr<vector<void*>> pointers =
				lo.load(startdir + "/FreeAX25_Config.so", { "run" });
		vector<void*>& _pointers{ *pointers.get() };
		auto run = (void(*)(std::string, Configuration&)) _pointers[0];
		run(xmlpath, env().configuration);
		env().logger.init();
		env().timerManager.init();
		if (env().logger.getLevel() >= LogLevel::DEBUG)
			Configuration::print(env().configuration);
	}
	catch (const std::exception& ex) {
		env().logError(string("Exception: ") + ex.what());
		return EXIT_FAILURE;
	}
	catch (...) {
		env().logError(string("Unidentified error"));
		return EXIT_FAILURE;
	}

	// Start operation:
	try {
		// Load plugins:
		for (UniquePointerDictIterator<Plugin> iter = env().configuration.plugins.begin();
				iter != env().configuration.plugins.end(); ++iter)
		{
			iter->second.get()->load();
		} // end //

		// Init plugins:
		for (UniquePointerDictIterator<Plugin> iter = env().configuration.plugins.begin();
				iter != env().configuration.plugins.end(); ++iter)
		{
			iter->second.get()->init();
		} // end //

		// Start plugins:
		for (UniquePointerDictIterator<Plugin> iter = env().configuration.plugins.begin();
				iter != env().configuration.plugins.end(); ++iter)
		{
			iter->second.get()->start();
		} // end //

		env().logInfo("Start operation");
		// Start heart beat:
		env().timerManager.start();
		// The program is finished when the heart beat stops:
		env().timerManager.join();
	}
	catch (const std::exception& ex) {
		env().logError(string("Exception: ") + ex.what());
		return EXIT_FAILURE;
	}
	catch (...) {
		env().logError(string("Unidentified error"));
		return EXIT_FAILURE;
	}

	env().logDebug("Exit program");
	return EXIT_SUCCESS;
}
