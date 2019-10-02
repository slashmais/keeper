
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

#ifndef _dbkeep_h_
#define _dbkeep_h_

#include <dbsqlite3/dbsqlite3.h>
#include <utilfuncs/utilfuncs.h>
#include <map>

struct Monitor;
struct Monitors;

struct DBKeep : public DBsqlite3
{
	virtual ~DBKeep() {}

	bool ImplementSchema();
	bool Save(Monitor &B);
	bool Load(Monitors &BS);
	bool Delete(Monitor &B);

};

extern DBKeep DBK;

#endif
