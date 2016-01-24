/*
 * NativeMIDIWin.h
 *
 *  Created on: Mar 8, 2012
 *  Author: dkadrioski
 */

#ifndef NATIVEMIDIWIN_H_
#define NATIVEMIDIWIN_H_

#include "FlashRuntimeExtensions.h" // import the adobe headers

	__declspec(dllexport) void initializer(void** extData, FREContextInitializer* ctxInitializer, FREContextFinalizer* ctxFinalizer);
	__declspec(dllexport) void finalizer(void* extData);


#endif /* NATIVEMIDIWIN_H_ */
