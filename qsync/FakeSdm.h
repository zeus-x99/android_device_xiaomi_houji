#pragma once

extern "C" {
int sdmInit(void*) asm("_ZN3sdm14DisplayBuiltIn4InitEv");
int sdmDeinit(void*) asm("_ZN3sdm14DisplayBuiltIn6DeinitEv");
int sdmDestroy(void*, void*) asm("_ZN3sdm8CoreImpl14DestroyDisplayEPNS_16DisplayInterfaceE");
int sdmMode(void*, int) asm("_ZN3sdm14DisplayBuiltIn12SetQSyncModeENS_9QSyncModeE");
int sdmLocked(void*, int) asm("_ZN3sdm14DisplayBuiltIn18SetQSyncModeLockedENS_9QSyncModeE");
int sdmPower(void*, int, bool, void*) asm("_ZN3sdm14DisplayBuiltIn15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE");
int sdmBasePower(void*, int, bool, void*) asm("_ZN3sdm11DisplayBase15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE");
int sdmGetPower(void*, int*) asm("_ZN3sdm11DisplayBase15GetDisplayStateEPNS_12DisplayStateE");
int sdmGetMode(void*, int*) asm("_ZN3sdm14DisplayBuiltIn12GetQSyncModeEPNS_9QSyncModeE");
int sdmGetId(void*, int*) asm("_ZN3sdm11DisplayBase12GetDisplayIdEPi");
bool sdmIsPrimary(void*) asm("_ZN3sdm11DisplayBase16IsPrimaryDisplayEv");

void* fakeCreate();
void fakeSetPrimary(void*, bool);
void fakeDelete(void*);
int fakeRequested(void*);
bool fakeBacklight(void*);
void fakeReject(void*, int mode, int count);
void fakePauseNext(void*);
bool fakeWaitPaused(void*);
void fakeResume(void*);
bool fakeDestroyed(void*);
}
