
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

#include "kprglob.h"
#include <utilfuncs/utilfuncs.h>
#include <unistd.h>
#include <sys/time.h>
#include <sstream>
#include <fstream>

bool bVERBOSE_LOG;
bool bDEBUG_LOG;

const float VERSION=1.0; // major . revision
const std::string Version() { return ttos<float>(VERSION); }

const std::string DateTime(bool bmillisec)
{
	int m,d,H,M,S,ms;
	std::stringstream ss;
	time_t t;
	struct tm tmcur;

	time(&t);
	tmcur=*localtime(&t);
	m=(tmcur.tm_mon+1);	d=tmcur.tm_mday;
	H=tmcur.tm_hour;	M=tmcur.tm_min;		S=tmcur.tm_sec;

	ss	<< (1900+tmcur.tm_year) << ((m<=9)?"0":"") << m
		<< ((d<=9)?"0":"") << d << "-" << ((H<=9)?"0":"") << H << ":"
		<< ((M<=9)?"0":"") << M << ":" << ((S<=9)?"0":"") << S;
	if (bmillisec)
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		ms=(tv.tv_usec % 10000L);
		ss << "." << ms;
	}
	return ss.str();
}

void write_log(std::string sData) { file_append(LOG_FILE, spf(DateTime(true), "\t", sData, "\n")); }
bool Report_Error(std::string serr) { LogLog("ERROR: ", serr); if (isatty(1)) tellerror(serr, "\n"); return false; }

bool wildcard_match(std::string wcs, std::string sval)
{ //wcs=>e.g.: "*~, *.tmp, *.temp, ?-*.bup, *.~*~" ==comma-delimited wildcard-templates
	std::vector<std::string> vex;
	int v=0, n, fn=sval.size();
	bool b=false;
	vex.clear();
	n=(int)desv<std::string>(wcs, ',', vex, false);
	while (!b&&(v<n))
	{
		std::string sw=vex.at(v++);
		int f=0, w=0, wn=sw.length();
		while ((f<fn)&&(w<wn) && (sval.at(f)==sw.at(w))) { f++; w++; }
		if ((w==wn)&&(f==fn)) { b=true; continue; }
		if ((w==wn)||(f>=fn)) continue;
		while ((f<fn)&&(w<wn))
		{
			if (sw.at(w)=='*') { while ((w<wn)&&((sw.at(w)=='*'))) w++; if (w==wn) { b=true; continue; }}
			if (sw.at(w)=='?')
			{
				while ((w<wn)&&(f<fn)&&((sw.at(w)=='?'))) { w++; f++; }
				if ((w==wn)&&(f==fn)) { b=true; continue; }
				if ((w==wn)||(f==fn)) continue;
			}
			std::string sp="";
			while ((w<wn)&&((sw.at(w)!='*')&&(sw.at(w)!='?'))) { sp+=sw.at(w); w++; }

			f=sval.find(sp, f);
			if (f==(int)std::string::npos) { f=fn+1; continue; }
			f+=sp.length();
			if ((w==wn)&&(f==fn)) { b=true; continue; }
			if ((w==wn)||(f==fn)) continue;
		}
		if ((w==wn)&&(f==fn)) { b=true; continue; }
	}
	return b;
}

//----------------------------------------------------------------------------------------------

QDConfig BUP_CONFIG;
