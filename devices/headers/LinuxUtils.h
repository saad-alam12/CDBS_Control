/*
 * LinuxUtils.h
 *
 *  Created on: Sep 13, 2018
 *      Author: Francesco Guatieri
 */

#ifndef FGLIBRARIES_EXPORTED_LINUXUTILS_H_
#define FGLIBRARIES_EXPORTED_LINUXUTILS_H_

const string TermColor_Default = "\x1B[0m";
const string TermColor_Brown   = "\x1B[38;5;130m";
const string TermColor_Red     = "\x1B[38;5;9m";
const string TermColor_Orange  = "\x1B[38;5;3m";
const string TermColor_Yellow  = "\x1B[38;5;11m";
const string TermColor_Green   = "\x1B[38;5;10m";
const string TermColor_Blue    = "\x1B[38;5;69m";
const string TermColor_Purple  = "\x1B[38;5;13m";
const string TermColor_Gray    = "\x1B[38;5;8m";
const string TermColor_White   = "\x1B[38;5;15m";

#include "FileUtils.h"

string ExtractExecutablePath(string CommandLine) {
	bool Escaping = false;
	for(int i = 0; i < CommandLine.size(); ++i) {
		if(Escaping)
			{ Escaping = false; continue; };
		if(CommandLine[i] == '\\')
			{ Escaping = true; continue; };
		if(CommandLine[i] == ' ' || CommandLine[i] == 0) {
			CommandLine = CommandLine.substr(0,i);
			break;
		};
	};

	return CommandLine;
}

string ExtractBinaryName(string CommandLine) {
	//cout << "'" << CommandLine << "'\n";
	CommandLine = ExtractExecutablePath(CommandLine);

	int CutAt = 0;
	bool Escaping = false;
	for(int i = 0; i < CommandLine.size(); ++i) {
		if(Escaping)
			{ Escaping = false; continue; };
		if(CommandLine[i] == '\\')
			{ Escaping = true; continue; };
		if(CommandLine[i] == '/') CutAt = i+1;
	};

	CommandLine = CommandLine.substr(CutAt);

	//cout << "\t-> '" << CommandLine << "'\n\n";

	return CommandLine;
}

vector<int> ListAllPIDs() {
	vector<string> PIDs = ListSubfolders("/proc/");
	vector<int>    TempRes;
	TempRes.reserve(PIDs.size());
	for(auto& p:PIDs) {
		bool Skip = false;
		for(auto& c:p)
			if(!isdigit(c)) {
				Skip = true;
				break;
			};
		if(Skip) continue;
		TempRes.push_back(stoi(p));
	}
	return TempRes;
}

string GetRawCommandline(int PID = getpid()) {
	string ProcFile = "/proc/" + itos(PID) + "/cmdline";
	if(!FileExists(ProcFile)) {
		Warn("Unable to check commandline for PID " + itos(PID) + ".");
		return "";
	};
	return ReadFile(ProcFile);
}

string GetCommandline(int PID = getpid()) {
	string ProcFile = "/proc/" + itos(PID) + "/cmdline";
	if(!FileExists(ProcFile)) {
		Warn("Unable to check commandline for PID " + itos(PID) + ".");
		return "";
	};
	string CL = ReadFile(ProcFile);
	for(auto& c:CL) if(!c) c = 32;
	return CL;
}

vector<string> ListAllCommandlines() {
	vector<int> PIDs = ListAllPIDs();
	vector<string> TempRes;
	for(auto& p:PIDs) {
		string s = GetCommandline(p);
		if(s.size()) TempRes.push_back(s);
	}
	return TempRes;
}

vector<string> ListAllRunningBinaries() {
	vector<int> PIDs = ListAllPIDs();
	vector<string> TempRes;
	for(auto& p:PIDs) {
		string s = GetCommandline(p);
		if(!s.size()) continue;
		TempRes.push_back(ExtractBinaryName(s));
	}
	return TempRes;
}

vector<int> ListPreviousInstances() {
	int MyPID = getpid();
	string myCommandline = GetRawCommandline(MyPID);

	string myBinary = ExtractBinaryName(myCommandline);

	vector<int> TempRes;
	vector<int> AllPIDs = ListAllPIDs();

	for(auto& p:AllPIDs) {
		if(p == MyPID) continue;
		string CL = GetRawCommandline(p);

		if(ExtractBinaryName(CL) == myBinary)
			TempRes.push_back(p);
	};

	return TempRes;
}

bool AmIRoot() {
	uid_t uid  = getuid();
	uid_t euid = geteuid();
	return uid == 0 || euid == 0;
}

/*vector<string> ProgramsRunning() {
	vector<string> TempRes;

	redi::ipstream Pipe;
	Pipe.open("ps aux", ios::in);

	if(!Pipe) Utter("Unable to open pipe to determine daemon current running status.\n");

	while(!Pipe.eof()) {
		string Buffer;
		getline(Pipe, Buffer);
		if(Buffer.length()) TempRes.push_back(Buffer);
	};

	Pipe.close();

	return TempRes;
}*/

#endif /* FGLIBRARIES_EXPORTED_LINUXUTILS_H_ */
