
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

#include "dbkeeper.h"
#include <string>
#include "monitor.h"

DBKeep DBK; //main db

bool DBKeep::ImplementSchema()
{
	bool b=bDBOK;
	DBResult RS;
	std::string sSQL;

	if (b)
	{
		sSQL = "CREATE TABLE IF NOT EXISTS IDS (name, id, del)";
		ExecSQL(&RS, sSQL); //create/reuse ids: init_ids(tabelname) once after CREATE TABLE.. and new_id() / del_id() after
		b=NoError(this);
	}
	
	if (b) b = (MakeTable("sources", "id, sourcedir, direxcludes, fileexcludes")&&init_ids("sources"));

//	if (b) b = MakeTable("backups", "id, bupdir, dts"); ... bupdir is kept in config-file
//s'thing like dts last bup maybe ...? can get file-time for last entry in Z-dir...

	return b;
}

bool DBKeep::Save(Monitor &B)
{
	std::string sSQL;
	DBResult RS;
	if (B.id)
	{
		sSQL=spf("UPDATE sources SET",
				" sourcedir = ", SQLSafeQ(B.sourcedir),
				", direxcludes = ", (B.direxcludes.empty()?"''":SQLSafeQ(B.direxcludes)),
				", fileexcludes = ", (B.fileexcludes.empty()?"''":SQLSafeQ(B.fileexcludes)),
				" WHERE id = ", B.id);
		ExecSQL(&RS, sSQL);
	}
	else
	{
		B.id=new_id("sources");
		sSQL=spf("INSERT INTO sources(id, sourcedir, direxcludes, fileexcludes) VALUES(",
					B.id,
					", ", SQLSafeQ(B.sourcedir),
					", ", SQLSafeQ(B.direxcludes),
					", ", SQLSafeQ(B.fileexcludes), ")");
		ExecSQL(&RS, sSQL);
	}
	return (NoError(this));
	
}

bool DBKeep::Load(Monitors &BS)
{
	std::string sSQL;
	DBResult RS;
	size_t i=0,n;
	n=ExecSQL(&RS, "SELECT * FROM sources");
	if (NoError(this))
	{
		BS.clear();
		while (i<n)
		{
			Monitor M;
			M.id = stot<DB_ID_TYPE>(RS.GetVal("id", i));
			M.sourcedir = SQLRestore(RS.GetVal("sourcedir", i));
			M.direxcludes = SQLRestore(RS.GetVal("direxcludes", i));
			M.fileexcludes = SQLRestore(RS.GetVal("fileexcludes", i));
			BS.push_back(M);
			i++;
		}
		return true;
	}
	return false;
}

bool DBKeep::Delete(Monitor &M)
{
	if (M.id)
	{
		std::string sSQL;
		DBResult RS;
		sSQL=spf("DELETE FROM sources WHERE id = ", M.id);
		ExecSQL(&RS, sSQL);
		if (NoError(this)) del_id("sources", M.id);
		return NoError(this);
	}
	SetLastError("cannot delete monitor: invalid id");
	return false;
}


