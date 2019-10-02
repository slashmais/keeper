
/*
    This file is part of the "keeper" application.

    "keeper" - Copyright 2018 Marius Albertyn

    "keeper" is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    "keeper" is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with "keeper" in a file named COPYING.
    If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _monitor_h_
#define _monitor_h_

#include <utilfuncs/utilfuncs.h>
#include <dbsqlite3/dbsqlite3types.h>
#include "kprglob.h"
#include <string>
#include <vector>
#include <map>
#include <thread>


struct Monitor
{
	int fdInotify;
	DB_ID_TYPE id;
	std::string sourcedir;
	std::string direxcludes;
	std::string fileexcludes;
	
	///...todo...
	bool bIsExtracting; //CYA when extracting to a monitored sourcedir
	
	std::thread this_monitor;
	
	void clear() { id=0; sourcedir.clear(); direxcludes.clear(); fileexcludes.clear(); bIsExtracting=false; }
	bool is_excluded_dir(const std::string &sdir);
	bool is_excluded_file(const std::string &sfile);
	
//	bool sync_compress(DTStamp dts, const std::string &F, const std::string &T, const DirEntries &md);
	bool sync_compress(const std::string &hdts, const std::string &F, const std::string &T, const DirEntries &md);
	bool sync_compress(const std::string &F, const std::string &T);

	bool sync_backup(const std::string &sbupdir); //get bup & source in same state
	bool sync_source();
	
	bool IsExtracting() { return bIsExtracting; }
	void SetExtracting(bool b=true) { bIsExtracting=b; }

	Monitor();
	Monitor(const Monitor &B);
	virtual ~Monitor();
	
	Monitor& operator=(const Monitor &B);
	
	bool Save();

	bool DoBackup(const std::string &sfile);

	static void do_monitor(Monitor *pMon);
	bool Start();
	bool Stop();

};

struct Monitors : std::vector<Monitor>
{
	void init_clear() { clear(); } //???there was some reason somewhere...
	void exit_clear();

	bool realize_bupdir(Monitor &M, const std::string &sBupDir);

	Monitors()			{ init_clear(); }
	virtual ~Monitors()	{ exit_clear(); }

	bool Load();
	
	bool AddUpdate(const std::string &src, const std::string &fex="", const std::string &dex="");

	bool Remove(const std::string &sSrcDir);

	Monitor* GetMonitor(const std::string &sSrcDir);

	bool StartAll();
	bool StopAll();
	
};

bool open_inotify();
bool close_inotify();
int get_fdinotify();
void init_bsc_watches();
bool IsMonitoring();

#endif
