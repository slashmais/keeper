
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

#ifndef _bscglobals_h_
#define _bscglobals_h_

#include <string>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <map>
#include <qdconfig/qdconfig.h>

extern std::string KEEPER_META_PATH;
extern std::string CONFIG_FILE;
extern std::string LOG_FILE;
extern bool bVERBOSE_LOG;
extern bool bDEBUG_LOG;

extern std::string DEF_BUP_DIR;

const std::string Version();
const std::string DateTime(bool bmillisec=false);
void write_log(std::string sData); //used by LogLog(..)
bool Report_Error(std::string serr);
bool wildcard_match(std::string wcs, std::string sval);

template<typename...P> void w_t_f(std::ostream &os) {}
template<typename H, typename...P> void w_t_f(std::ostream &os, H h, P...p) { os << h; w_t_f(os, p...); }
template<typename...P> void write_to_file(const std::string sfile, P...p) { std::ofstream ofs(sfile, std::ios_base::app); w_t_f(ofs, p...); }
template<typename...P> void LogLog(P...p) { std::ostringstream oss(""); w_t_f(oss, p...); write_log(oss.str()); }
template<typename...P> void LogDebug(P...p) { if (bDEBUG_LOG) write_to_file(LOG_FILE+".DEBUG", p..., "\n"); }
template<typename...P> void LogVerbose(P...p) { if (bVERBOSE_LOG) write_to_file(LOG_FILE+".VERBOSE", p..., "\n"); }

//struct BupConfig
//{
//	std::map<std::string, std::string> m;
//	BupConfig() { m.clear(); }
//	~BupConfig() { m.clear(); }
//	void setval(const std::string &k, const std::string &v);
//	const std::string getval(const std::string &k);
//	bool Save();
//	bool Load();
//};
//	
//extern BupConfig BUP_CONFIG;

extern QDConfig BUP_CONFIG;

#endif
