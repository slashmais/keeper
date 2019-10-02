
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
#include <utilfuncs/utilfuncs.h>
#include <compression/compression.h>
#include <unistd.h>
#include <cerrno>
#include <functional>
#include <sys/inotify.h>
#include <mutex>

extern bool bVERBOSE_LOG;

bool bStopMonitoring;
unsigned int monitor_count=0;
std::mutex MUX_monitor_count;
bool IsMonitoring() { return (monitor_count>0); }

Monitor::Monitor()						{ clear(); }

Monitor::Monitor(const Monitor &B)
{
	fdInotify=B.fdInotify;
	id=B.id;
	sourcedir=B.sourcedir;
	fileexcludes=B.fileexcludes;
	direxcludes=B.direxcludes;
	bIsExtracting=B.bIsExtracting;
}

Monitor::~Monitor()						{ clear(); }

bool Monitor::is_excluded_dir(const std::string &sdir) { return wildcard_match(direxcludes, sdir); }

bool Monitor::is_excluded_file(const std::string &sfile) { return wildcard_match(fileexcludes, sfile); }

//bool Monitor::sync_compress(DTStamp dts, const std::string &F, const std::string &T, const DirEntries &md)
bool Monitor::sync_compress(const std::string &hdts, const std::string &F, const std::string &T, const DirEntries &md)
{
	bool b=true;
	if (!md.empty())
	{
		std::string sf, st, sz;
		auto it=md.begin();
		while (b&&(it!=md.end()))
		{
			sf=path_append(F, it->first);
			st=path_append(T, it->first);
			if (isdirtype(it->second)&&!is_excluded_dir(sf))
			{
				DirEntries me;
				if (dir_read(sf, me)) { if ((b=dir_create(st))) { b=sync_compress(hdts, sf, st, me); }}
				else Report_Error(spf("cannot sync '", sf, "' -", filesys_error())); //, false); //and carry-on
			}
			if (isfiletype(it->second)&&!is_excluded_file(sf))
			{
				if (file_exist(st)&&!file_compare(sf, st))
				{
					std::string zdir=spf(st, ".Z");
					if (path_realize(zdir))
					{
						sz=path_append(zdir, spf("DT", hdts, "_", file_name(st), ".z"));
						compress_file(st, sz); if (file_exist(sz)) file_delete(st); //equivalent to 'move'
					}
				}
				if (!file_exist(st)||!file_compare(sf, st)) b=file_copy(sf, st);
			}
			//ignore if not directory or regular file (=> device/pipe/link/socket) ..
			// or put handlers here if needed..
			it++;
		}
	}
	return b;
}

bool Monitor::sync_compress(const std::string &F, const std::string &T)
{
	if (!path_realize(T)) return false;
//	DTStamp dts=dt_stamp();
	std::string hdts=h_dt_stamp();
	DirEntries md;
	dir_read(F, md);
	return sync_compress(hdts, F, T, md);
}

bool Monitor::sync_backup(const std::string &sbupdir)
{
//	DTStamp dts=dt_stamp();
	std::string hdts=h_dt_stamp();
	DirEntries md;
	dir_read(sourcedir, md);
	return sync_compress(hdts, sourcedir, sbupdir, md);
}

bool Monitor::sync_source()
{
	std::string B;
	if (!dir_exist(sourcedir)) { return Report_Error(spf("cannot find source dir: '", sourcedir, "'")); }
	B=path_append(DEF_BUP_DIR, path_name(sourcedir));
	if (!path_realize(B)) return false; //ensure it exists...?!?!
	return sync_backup(B);
}

/* is this needed? */
Monitor& Monitor::operator=(const Monitor &B)
{
	fdInotify=B.fdInotify;
	id=B.id;
	sourcedir=B.sourcedir;
	fileexcludes=B.fileexcludes;
	direxcludes=B.direxcludes;
	bIsExtracting=B.bIsExtracting;
	return *this;
}


bool Monitor::Save() { return DBK.Save(*this); }

bool Monitor::DoBackup(const std::string &sfile)
{
	if (!isfile(sfile)||is_excluded_file(sfile)) return false; //backup only regular files
	//DTStamp dts=dt_stamp();
	std::string hdts=h_dt_stamp();
	std::string st, sz, B;
	bool b=true;
	B=path_append(DEF_BUP_DIR, path_name(sourcedir));
	st=path_append(B, sfile.substr(sourcedir.size()+1));
	if (file_exist(st)&&!file_compare(sfile, st))
	{
		std::string zdir=st;
		zdir+=".Z";
		if (path_realize(zdir))
		{
			sz=path_append(zdir, spf("DT", hdts, "_", file_name(st), ".z"));
			//if (file_exist(sz)) file_backup(sz);
			compress_file(st, sz); if (file_exist(sz)) file_delete(st); //equivalent to 'move'
		}
	}
	if (!file_exist(st)||!file_compare(sfile, st)) b=file_copy(sfile, st); //current
	if (!b) Report_Error(spf("cannot backup '", sfile, "' to '", st, "' - ", filesys_error()));
	return b;
}


//start & stop called from monitors...
bool Monitor::Start() { if (!this_monitor.joinable()) { this_monitor=std::thread(do_monitor, this); } return this_monitor.joinable(); }
bool Monitor::Stop() { if (this_monitor.joinable()) this_monitor.join(); return true; }

//--------------------------------------------------------------------------------------------------
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

std::map<int, std::pair<Monitor*, std::string> > BSC_WatchList;

void init_bsc_watches() { BSC_WatchList.clear(); }

void scansubs(Monitor *pM, std::string &sdir, std::function<bool(Monitor*, std::string&)> f) //add sub-dirs to watchlist...
{
	auto get_subdirs=[&](std::string &sdir, std::vector<std::string> &v)->bool
		{
			////std::map<std::string, int> md;
			//std::map<std::string, std::experimental::filesystem::v1::file_type> md;

			DirEntries md;
			v.clear();
			if (dir_read(sdir, md))
			{
				if (!md.empty()) for (auto p:md) { if (isdirtype(p.second)) v.push_back(path_append(sdir, p.first)); }
			}
			return !v.empty();
		};
	std::vector<std::string> v;
	if (get_subdirs(sdir, v)) { for (auto d:v) { if (f(pM, d)) scansubs(pM, d, f); }}
}

void Monitor::do_monitor(Monitor *pMon)
{
	uint32_t MONITOR_MASK=(IN_MODIFY|IN_CREATE|IN_MOVED_TO|IN_MOVE_SELF|IN_DONT_FOLLOW);
	int wd;
	int fdI=pMon->fdInotify;

	auto iswatch=[&](std::string &sdir)->bool
	{
		auto it=BSC_WatchList.begin();
		while (it!=BSC_WatchList.end()) { if (seqs(it->second.second, sdir)) return true; it++; }
		return false;
	};
	
	auto inawerr=[&](int en, const std::string sd)->bool
		{
			std::string serr;
			serr=spf("\n\t>\tinotify_add_watch '", sd, "'");
			switch(en)
			{
				case EACCES:	serr+=":\n\t>\tRead access is not permitted."; break;
				case ENOENT:	serr+=":\n\t>\tA directory component in pathname does not exist or is a dangling symbolic link."; break;
				case EBADF:		serr+=":\n\t>\tThe given file descriptor is not valid."; break;
				case EFAULT:	serr+=":\n\t>\tpathname points outside of the process's accessible address space."; break;
				case EINVAL:	serr+=":\n\t>\tThe given event mask contains no legal events; or fd is not an inotify file descriptor."; break;
				case ENOMEM:	serr+=":\n\t>\tInsufficient kernel memory was available."; break;
				case ENOSPC:	serr+=":\n\t>\tThe user limit on inotify watches was reached or the kernel failed to allocate a resource."; break;
				default:		serr+=":\n\t>\tUnknown error"; break;
			}
			if ((errno==ENOMEM)||(errno==ENOSPC)) serr+="\n\t>\t(In case of a limit- or memory-related error, inspect:"
														"\n\t>\t\tsudo sysctl fs.inotify.max_user_watches\n\t>\tand"
														"\n\t>\t\tsudo sysctl fs.inotify.max_user_instances"
														"\n\t>\tand adjust the values.)";
			return Report_Error(serr);
		};

	auto watchit=[&](Monitor *pM, std::string &sdir)->bool
		{
			if (pM->is_excluded_dir(sdir)) return false;
			if (iswatch(sdir)) return true;
			wd=inotify_add_watch(fdI, sdir.c_str(), MONITOR_MASK);
			if (wd>=0)
			{
				BSC_WatchList[wd]=std::pair<Monitor*, std::string>(pM, sdir);
				std::unique_lock<std::mutex> LOCK(MUX_monitor_count); ++monitor_count; LOCK.unlock();
				LogVerbose("[wd=", wd, "] Monitor set for '", sdir, "'");
				return true;
			}
			else return inawerr(errno, sdir);
		};
	
	//create watches-------------------------------------------------------
	watchit(pMon, pMon->sourcedir);
	scansubs(pMon, pMon->sourcedir, watchit);

	//watch-loop-----------------------------------------------------------
	char EventBuf[BUF_LEN];
	struct inotify_event *pEvt;
	int n, length;
	Monitor *pM;
	std::string swdir;
	while (!bStopMonitoring)
	{
		length=read(fdI, EventBuf, BUF_LEN );
		if (length>0)
		{
			n=0;
			while (n<length)
			{
				pEvt=(struct inotify_event*)&EventBuf[n];
				if (BSC_WatchList.find(pEvt->wd)!=BSC_WatchList.end())
				{
					pM=BSC_WatchList[pEvt->wd].first;
					swdir=BSC_WatchList[pEvt->wd].second;
					if (!pM->IsExtracting())
					{
						if (pEvt->len)
						{
							std::string fd=path_append(swdir, pEvt->name);
							if ((pEvt->mask&IN_ISDIR)==IN_ISDIR)
							{
								if (((pEvt->mask&IN_CREATE)==IN_CREATE)||((pEvt->mask&IN_MOVED_TO)==IN_MOVED_TO)) //new sub-dir - add to watchlist..
								{
									std::string B;
									B=path_append(DEF_BUP_DIR, path_name(pM->sourcedir));
									std::string td=path_append(B, fd.substr(pM->sourcedir.size()+1));
									pM->sync_compress(fd, td);
									if (watchit(pM, fd)) scansubs(pM, pM->sourcedir, watchit);
								}
							}
							else
							{
								if (isfile(fd)) pM->DoBackup(fd);
							} //ignore if not a regular file
						}
					}
				}
				n+=(EVENT_SIZE+pEvt->len);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	//clean-up-------------------------------------------------------------
	std::unique_lock<std::mutex> LOCK(MUX_monitor_count);
		while (!BSC_WatchList.empty())
		{
			auto pW=BSC_WatchList.begin();
			inotify_rm_watch(fdI, pW->first);
			--monitor_count;
			std::this_thread::sleep_for(std::chrono::milliseconds(10)); //give inotify time to cleanup..?
			BSC_WatchList.erase(pW);
		}
	LOCK.unlock();

}



