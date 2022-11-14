
#pragma once
#include "pch.h"
#include <vector>

void t_FromSjis(tstring *pDest, const void *pSrc, const size_t srcAreaSize);
size_t t_ToWiden(const std::string &src, std::wstring *pDest);
const std::vector<tstring> &t_CreateStringArray(
	const tstring &sep, const tstring &target, std::vector<tstring> *pDest);
int t_Trimer(const int v, const int minv, const int maxv);
void t_MemSetZero(void *const p, const size_t size);
void t_ToNarrow(const std::wstring &src, std::string *pDest);
bool t_kbhit(void);
char t_getch(void);
void t_sleep(unsigned int msec);
uint64_t t_GetFileSize(const tstring &targetPath);
bool t_ReadFile(const tstring &targetPath, std::vector<uint8_t> **pp);
bool t_WriteFile(const tstring &targetPath, const std::vector<uint8_t> &dt);
bool t_WriteFileAdd(const tstring& targetPath, const uint8_t* pDt, const size_t s);
bool t_FileExist(const tstring& targetPath);
const tstring &t_GetModuleFileName(tstring *pName);
bool t_GetFileVersionInfo(
	const tstring &tergetPath,
	tstring *pNameStr, tstring *pVerNumberStr, tstring *pCopyrightStr);
