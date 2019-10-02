
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

#include "keeper.h"
#include <utilfuncs/utilfuncs.h>
#include "kprglob.h"
#include "monitor.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <compression/compression.h>


//=====Switch verbose- & debug-logs on when developing...
//bool bIS_DEV_MODE=true;
bool bIS_DEV_MODE=false;
//=====


volatile bool bQuit;
volatile bool bRestart;
std::mutex MUX;
std::condition_variable CVMain;

void signal_handler(int sig)
{
	if (sig==RESTART_SIGNAL)				{ bRestart=true; CVMain.notify_one(); }
	if ((sig==SIGINT)||(sig==STOP_SIGNAL))	{ bQuit=true; CVMain.notify_one(); }
	//for use while developing/debugging.. (use bIS_DEV_MODE to set default)
	if (sig==DEBUG_ON_SIGNAL)				{ bDEBUG_LOG=true; }
	if (sig==DEBUG_OFF_SIGNAL)				{ bDEBUG_LOG=false; }
	if (sig==VERBOSE_ON_SIGNAL)				{ bVERBOSE_LOG=true; }
	if (sig==VERBOSE_OFF_SIGNAL)			{ bVERBOSE_LOG=false; }
}

void show_help()
{
	telluser("\nReal-time _DEEP_ backup of regular files in directories;",
			 "\ndir-hierarchy is maintained; non-regular files are ignored.",
			 
			 (is_running())?spf("\n\n *** An instance of ", AppName(), " is active ***"):"",

			 "\n\nUsage: ", AppName(), " [OPTIONS]",
			 "\n\nOPTIONS:",
			 
			//Note: logverbose and/or logdebug as leading args (intended for use while developing/debugging)
			//		toggles logging and is optionally followed by the below args

			 "\n\n -? | --help",
			 "\n\tthis help",

			 "\n\n RESTART | STOP",
			 "\n\tRESTART stop, reload data, and restart ", AppName(),
			 "\n\tSTOP terminates ", AppName(),

			 //"\n\n \"source-dir\" \"csv-file-excl\" \"csv-dir-excl\"",  .......todo....think this through...
			 //"\n OR",
			 "\n\n -a | --add \"source-dir\" \"csv-file-excl\" \"csv-dir-excl\"",
			 "\n\tadd/update source and/or excludes to db and sync's to backup;",
			 "\n\tcsv-file-excl and csv-dir-excl:",
			 "\n\t    quoted csv-strings of wildcard-values for which matching entries",
			 "\n\t    must be excluded from backup; wilcards are '*' for range and '?'",
			 "\n\t    for single chars; (may be ommited or use empty quotes)",
			 "\n\tExample: \"../source/..\" \"*~, *.temp\" \"TMP, data/scratch\"",
			 "\n\t    will exclude from backup all files ending with a ~ or .temp in",
			 "\n\t    /source/ and it's sub-directories', and also exclude '/source/TMP'",
			 "\n\t    and '/source/data/scratch' sub-directories.",

			 "\n\n --buploc \"default-bup-location\"",
			 "\n\tchanges the default location where backups are created, all existing",
			 "\n\tbackups will be copied to the new default-location",

			 "\n\n --remove \"source-dir\"",
			 "\n\tremoves source-dir from db only, existing backups are not touched",
			 "\n\t(if you want to delete the backups, use your file-manager)",

			 "\n\n -l | --list",
			 "\n\tlists current backup sources and excludes",

			 "\n\n -x | --extract \"bup-file-name\" \"to-dir\"",
			 "\n\tuncompress the specified bup-file-name to \"/to-dir/file-name\"",

			 "\n\n");
}

void show_version() { telluser(AppName(), Version(), "\n"); }

enum //options
{
	OPT_NONE,
	OPT_ADD,
	OPT_BUP,
	OPT_REM,
	OPT_LST,
	OPT_EXT,
};

bool process_commandline(int c, char **pp)
{
	if (c==1) { if (is_running()) { show_help(); return true; } else { return false; }}
	
	int n=0, Opt{OPT_NONE};
	std::string s, args[3];
	auto nextc=[&](std::string &s)->bool{ if (++n<c) s=pp[n]; else s.clear(); return (n<c); };

	args[0]=args[1]=args[2]="";

	while (nextc(s))
	{
		TRIM(s, "- ");

		if (sieqs(s, "logverbose"))		{ bVERBOSE_LOG=!bVERBOSE_LOG; if (is_running()) PostVerbose(bVERBOSE_LOG); continue; }
		else if (sieqs(s, "logdebug"))	{ bDEBUG_LOG=!bDEBUG_LOG; if (is_running()) PostDebug(bDEBUG_LOG); continue; }
	
		if ((n==(c-1))&&!(seqs(s, "l")||sieqs(s, "list")))
		{
			if (sieqs(s, "restart"))	{ PostRestart(); return true; }
			else if (sieqs(s, "stop"))	{ PostQuit(); return true; }
			else						{ show_help(); return true; }
		}
		
		if (seqs(s, "a")||sieqs(s, "add"))			{ Opt=OPT_ADD; }
		else if (sieqs(s, "buploc"))				{ Opt=OPT_BUP; }
		else if (sieqs(s, "remove"))				{ Opt=OPT_REM; }
		else if (seqs(s, "l")||sieqs(s, "list"))	{ Opt=OPT_LST; }
		else if (seqs(s, "x")||sieqs(s, "extract"))	{ Opt=OPT_EXT; }
		else { show_help(); return true; }
		if (Opt!=OPT_NONE) { int i=n; while (nextc(s)) args[n-i-1]=s; }
	}

	if (Opt!=OPT_NONE)
	{
		Monitors Mons;
		Mons.Load();

		switch(Opt)
		{
			case OPT_ADD: { if (Mons.AddUpdate(args[0], args[1], args[2])) PostRestart(); } break;
			
			case OPT_BUP: //NOT TESTED!
				{
					std::string sold{DEF_BUP_DIR};
					std::string snew{args[0]};
					if (!seqs(sold, snew)&&dir_exist(snew)&&!issubdir(snew, sold))
					{
						if (dir_copy(sold, snew))
						{
							BUP_CONFIG.setval("defbupdir", snew);
							if (!BUP_CONFIG.Save()) { BUP_CONFIG.setval("defbupdir", sold); telluser("\nFailed to set default backup directory: ", snew, "\n"); }
						}
						else telluser("\nFailed to copy data from ", sold, " to ", snew, "\nDefault directory NOT changed");
					}
					else telluser("\nInvalid default backup directory: ", snew, "\n");
				} break;
			
			case OPT_REM: { Mons.Remove(args[0]); } break;
			
			case OPT_LST:
				{
					telluser("Default Backup-dir: ", DEF_BUP_DIR, "\n");
					if (Mons.size()>0)
					{
						for (auto M:Mons) { telluser(M.sourcedir,"\n\tfile-excludes: ", M.fileexcludes, "\n\tdir-excludes: ", M.direxcludes, "\n"); }
					}
					else telluser("\n(no current backups)\n");
				} break;
				
			case OPT_EXT:
				{
					std::string sf{args[0]}, st{args[1]}, sn;
					if (file_exist(sf)&&(sf[sf.size()-1]=='z'))
					{
						
						if (dir_exist(st))
						{
							size_t p;
							sn=path_name(sf);
							p=sn.find('_');
							if ((p!=std::string::npos)&&(p<(sn.size()-2)))
							{
								sn=sn.substr(p+1);
								if (sn.size()>2)
								{
									sn=sn.substr(0, sn.size()-2);
									st=path_append(st, sn);
									if (!decompress_file(sf, st)) telluser("Failed to uncompress ", sf, " to ", st);
								}
								else telluser("Not a valid compressed file: ", sf);
							}
							else telluser("Not a valid compressed file: ", sf);
						}
						else telluser("Directory does not exist: ", st);
					}
					else telluser("Not a compressed file: ", sf);
				} break;

		}
	}
	return true;
}

void set_signals()
{
	signal(RESTART_SIGNAL, signal_handler);
	signal(STOP_SIGNAL, signal_handler);
	signal(SIGINT, signal_handler);
	signal(DEBUG_ON_SIGNAL, signal_handler);
	signal(DEBUG_OFF_SIGNAL, signal_handler);
	signal(VERBOSE_ON_SIGNAL, signal_handler);
	signal(VERBOSE_OFF_SIGNAL, signal_handler);
}

int main(int argc, char *argv[])
{
	bVERBOSE_LOG=bIS_DEV_MODE;
	bDEBUG_LOG=bIS_DEV_MODE;
	
	AppName(argv[0]);
	
	//lazy! avoiding having to change the code all-over.. todo
	if (!seqs(AppName(), "keeper"))
	{
		telluser("This application's name must remain as 'keeper'\nelse things will break!");
		exit(1);
	}
	
	if (!InitBVC()) return tellerror("Cannot initialize ", AppName());
	bRestart=bQuit=false;
	if (process_commandline(argc, argv)) return 0;
	telluser(AppName(), " version ", Version(), " - starting..\n(check (if exists): ", LOG_FILE, " for reports)\n");
	if (StartBVC())
	{
		Monitors monitors;
		set_signals();
		bQuit=!monitors.StartAll();
		while (!bQuit)
		{
			if (bRestart)
			{
				LogVerbose("..stopping..");
				if (monitors.StopAll())
				{
					BUP_CONFIG.Load();
					DEF_BUP_DIR=BUP_CONFIG.getval("defbupdir");
					if (!dir_exist(DEF_BUP_DIR)) { Report_Error("Invalid configuration"); exit(1); }
					LogVerbose("..restarting.."); monitors.StartAll();
				}
				else LogLog("BUG!: active monitors");
				bRestart=false;
			}
			std::unique_lock<std::mutex> LOCK(MUX);
			CVMain.wait(LOCK);
		}
		monitors.StopAll();
		StopBVC();
	}
	else show_help();
	ExitBVC();
    return 0;
}

