
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

#ifndef _keep_h_
#define _keep_h_

#include <string>
#include <signal.h>
#include <map>

extern int RESTART_SIGNAL;// SIGUSR1
extern int STOP_SIGNAL; // SIGUSR2
extern int DEBUG_ON_SIGNAL; // SIGRTMIN+1
extern int DEBUG_OFF_SIGNAL; // SIGRTMIN+2
extern int VERBOSE_ON_SIGNAL; // SIGRTMIN+3
extern int VERBOSE_OFF_SIGNAL; // SIGRTMIN+4

void AppName(const std::string &sname);
const std::string AppName();

bool is_running();

bool InitBVC();
void ExitBVC();

bool StartBVC();
bool StopBVC();

bool PostRestart();
bool PostQuit();

void PostDebug(bool bOn);
void PostVerbose(bool bOn);

#endif
