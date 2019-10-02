
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

#include "monitor.h"
#include "dbkeeper.h"
#include "kprglob.h"
#include <unistd.h>
#include <sys/inotify.h>

extern bool bStopMonitoring;
int fdInotify=(-1);
int get_fdinotify() { return fdInotify; }

bool open_inotify()
{
	if (fdInotify==(-1)) fdInotify=inotify_init1(IN_NONBLOCK);
	if (fdInotify<0)
	{
		std::string e=spf("open_inotify error[", errno, "] ");
		switch(errno)
		{
			case EINVAL: e+="(inotify_init1()) An invalid value was specified in flags."; break;
			case EMFILE: e+="The user limit on the total number of inotify instances has been reached."; break;
			case ENFILE: e+="The system limit on the total number of file descriptors has been reached."; break;
			case ENOMEM: e+="Insufficient kernel memory is available."; break;
			default: e+="unknown error";
			
			/*
				if limit reached, see: https://unix.stackexchange.com/a/13757/2141
			*/
			
		}
		Report_Error(e);
	}
	return (fdInotify>(-1));
}

bool close_inotify() { if (fdInotify>(-1)) close(fdInotify); fdInotify=(-1); return true; }

void Monitors::exit_clear() { StopAll(); clear(); }

bool Monitors::Load()
{
	clear();
	return DBK.Load(*this);
}


bool Monitors::realize_bupdir(Monitor &M, const std::string &bupdir)
{

	///todo...think through...
	//...error - bup is sub of DEF_BUP_DIR => serious tail-chasing here...
	//...error - bup is sub of src => tail-chasing...
	//...error? - if src is parent of a current monitored src... (??? e.g.: bup subs to workbubs, & bup all to somewhere else
	//... same dev/part => warn -- ok, intent is for safe backup in case drive fails..(happened to me three times - fucking seagate shite - NEVER USE SEAGATE HARDDRIVES!)
	//...if parent of a bup then replaces bup OR error - must first remove bup then add parent ... prevent duplicating monitors...
	//...?

	if (issubdir(bupdir, M.sourcedir)) return false; //Report_Error("cannot backup to a sub-dir of the source-directory");
	if (!path_realize(bupdir)) return false; // return Report_Error(spf("cannot create backup-dir '", B, "'"));
	return M.sync_backup(bupdir);

}

bool Monitors::AddUpdate(const std::string &src, const std::string &fex, const std::string &dex)
{
	std::string S{src}, B;
	if (!dir_exist(S)) return Report_Error(spf("cannot find source dir: '", S, "'"));
	B=path_append(DEF_BUP_DIR, path_name(S));

	///...todo...
	//if (!check_syntax(direx)) return Report_Error(spf("invalid dir-excludes: '", direx, "'"));
	//if (!check_syntax(fileex)) return Report_Error(spf("invalid file-excludes: '", fileex, "'"));
	//...tough one...e.g.: wildcard-templates are sep'd by comma => template cannot contain commas, so..when is it a template & when not?
	///case of garbage-in-garbage-out? --- alternative is specify excl's separately one-by-one...(then comma's won't matter)

	Monitor *pM=GetMonitor(S);
	if (pM)
	{
		pM->fileexcludes=fex;
		pM->direxcludes=dex;
		if (pM->Save()) return pM->sync_backup(B);
		return Report_Error(spf("database problem with existing source: '", S, "'"));
	}
	else
	{
		Monitor M;
		M.sourcedir=S;
		M.fileexcludes=fex;
		M.direxcludes=dex;
		if (realize_bupdir(M, B))
		{
			if (M.Save()) { push_back(M); return true; }
			return Report_Error(spf("database problem with new source: '", S, "'"));
		}
		else return Report_Error(spf("problem with realizing backup: '", B, "'"));
	}
}

bool Monitors::Remove(const std::string &src)
{
	bool b=false;
	Monitor *pm=GetMonitor(src);
	if (pm) { if (DBK.Delete(*pm)) { if (StopAll()) b=StartAll(); }}
	return b;
}

Monitor* Monitors::GetMonitor(const std::string &sSrcDir)
{
	Monitor *pB=nullptr;
	auto it=begin();
	while (!pB&&(it!=end())) { if (seqs(sSrcDir, it->sourcedir)) pB=&(*it); it++; }
	return pB;
}

bool Monitors::StartAll()
{
	if (IsMonitoring()) StopAll();
	clear();
	if (!open_inotify()) return Report_Error("cannot initialize inotify");
	bStopMonitoring=false;
	init_bsc_watches();
	if (DBK.Load(*this)) //(re)load fresh from db
	{
		if (size()>0)
		{
			for (auto& M:(*this))
			{
				M.sync_source();
				M.fdInotify=fdInotify;
				M.Start();
			}
		}
		else LogVerbose("(nothing to monitor)");
		return true;
	}
	Report_Error(spf("Database: ", DBK.sDB, " - ", DBK.GetLastError()));
	return false;
}

bool Monitors::StopAll()
{
	if (size()>0) { bStopMonitoring=true; for (auto& M:(*this)) { M.Stop(); }}
	close_inotify();
	return !IsMonitoring();
}
