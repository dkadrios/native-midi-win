/*
 * NativeMIDIWin.c
 *
 *  Created on: Mar 8, 2012
 *      Author: dkadrioski
 */


#include "FlashRuntimeExtensions.h" // import the adobe headers
#include "StdLib.h"
#include "NativeMIDIWin.h"
#include "String.h"

#include "windows.h"
#include "stdio.h"
//#include "conio.h"
#include "mmsystem.h"

/* BASE SIGNATURE
FREObject myFunction(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]){
	FREObject result;

	return result;
}
*/

const uint8_t* IN_OPEN      = (const uint8_t*)"IN_OPEN";
const uint8_t* IN_CLOSE     = (const uint8_t*)"IN_CLOSE";
const uint8_t* IN_DATA      = (const uint8_t*)"IN_DATA";
const uint8_t* IN_LONGDATA  = (const uint8_t*)"IN_LONGDATA";
const uint8_t* IN_ERROR     = (const uint8_t*)"IN_ERROR";
const uint8_t* IN_LONGERROR = (const uint8_t*)"IN_LONGERROR";
const uint8_t* IN_MOREDATA  = (const uint8_t*)"IN_MOREDATA";
const uint8_t* IN_UNKNOWN   = (const uint8_t*)"IN_UNKNOWN";

const uint8_t* MSG_INFO     = (const uint8_t*)"MSG_INFO";
const uint8_t* MSG_ERROR    = (const uint8_t*)"MSG_ERROR";

FREContext dllContext;
HMIDIIN hMidiInDevice = NULL;
HMIDIOUT hMidiOutDevice = NULL;

MIDIHDR midiHdr;

/*	A buffer to hold incoming SysEx bytes. */
unsigned char SysXBuffer[256];

/* A flag to indicate whether we're currently receiving a SysEx message */
unsigned char SysXFlag = 0;

//------------------------------------------------------------------------------
void dispatchEvent( const uint8_t* level, const uint8_t* code ){
	FREDispatchStatusEventAsync( dllContext, code, level );
}
//------------------------------------------------------------------------------
void showMessage(const uint8_t* msg){
	dispatchEvent( MSG_INFO, msg );
}
//------------------------------------------------------------------------------
void showError(const uint8_t* msg){
	dispatchEvent( MSG_ERROR, msg );
}
//------------------------------------------------------------------------------
void outputMidiInErrorMsg(unsigned long err){
	int32_t BUFFERSIZE = 200;
	char buffer[BUFFERSIZE];

	char str[1024];

	if (!(err = midiInGetErrorText(err, &buffer[0], BUFFERSIZE))){
		sprintf(str, "%s\r\n", &buffer[0]);
	}else if (err == MMSYSERR_BADERRNUM){
		sprintf(str, "Strange error number returned!\r\n");
	}else if (err == MMSYSERR_INVALPARAM){
		sprintf(str, "Specified pointer is invalid!\r\n");
	}else{
		sprintf(str, "Unable to allocate/lock memory!\r\n");
	}

	showError( (const uint8_t*)str );
}
//------------------------------------------------------------------------------
void outputMidiOutErrorMsg(unsigned long err){
	int32_t BUFFERSIZE = 120;

	char buffer[BUFFERSIZE];

	char str[1024];

	if (!(err = midiOutGetErrorText(err, &buffer[0], BUFFERSIZE))){
		sprintf(str, "%s\r\n", &buffer[0]);
	}else if (err == MMSYSERR_BADERRNUM){
		sprintf(str, "Strange error number returned!\r\n");
	}else{
		sprintf(str, "Specified pointer is invalid!\r\n");
	}

	showError( (const uint8_t*)str );
}
//------------------------------------------------------------------------------
void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2){
	char str[1024];

	LPMIDIHDR		lpMIDIHeader;
	unsigned char *	ptr;
	TCHAR			buffer[80];
	unsigned char 	bytes;

	switch(wMsg) {
		case MIM_OPEN:
			//dispatchEvent( IN_OPEN, (const uint8_t*) "MIM_OPEN" );
			break;

		case MIM_CLOSE:
			//dispatchEvent( IN_CLOSE, (const uint8_t*) "MIM_CLOSE" );
			break;

		case MIM_DATA:
			// Used for most midi messages.
			//dwParam1 is dwMidiMessage (4 bytes)
			//High word
			//		High-order byte	Not used.
			//		Low-order byte	Contains a second byte of MIDI data (when needed).
			//
			//Low word
			//		High-order byte	Contains the first byte of MIDI data (when needed).
			//		Low-order byte	Contains the MIDI status.

			// dwParam2 is dwTimestamp
			// milliseconds since midiInStart was called
			sprintf( str, "dwParam1=%ld, dwParam2=%ld", dwParam1, dwParam2 );

			dispatchEvent( IN_DATA, (const uint8_t*) str );
			break;

		case MIM_LONGDATA:
			/* Received all or part of some System Exclusive message */
			/* If this application is ready to close down, then don't midiInAddBuffer() again */
			if (!(SysXFlag & 0x80)){
				/*	Assign address of MIDIHDR to a LPMIDIHDR variable. Makes it easier to access the
					field that contains the pointer to our block of MIDI events */
				lpMIDIHeader = (LPMIDIHDR)dwParam1;

				/* Get address of the MIDI event that caused this call */
				ptr = (unsigned char *)(lpMIDIHeader->lpData);

				/* Is this the first block of System Exclusive bytes? */
				if (!SysXFlag){
					/* Indicate we've begun handling a particular System Exclusive message */
					SysXFlag |= 0x01;
				}

				/* Is this the last block (ie, the end of System Exclusive byte is here in the buffer)? */
				if (*(ptr + (lpMIDIHeader->dwBytesRecorded - 1)) == 0xF7){
					/* Indicate we're done handling this particular System Exclusive message */
					SysXFlag &= (~0x01);
				}

				/* Display the bytes -- 16 per line */
				bytes = 16;

				while((lpMIDIHeader->dwBytesRecorded--)){
					// This formatting of the string is pretty pointless, but leaving it alone.
					// The original example outputted everything to the console in pretty-print.
					// I don't want to mess with this and risk breaking something.
					// Overall this works, but could be more efficient.  NOT A BIG DEAL.
					if (!(--bytes)){
						sprintf(&buffer[0], "0x%02X\r\n", *(ptr)++);
						bytes = 16;
					}else{
						sprintf(&buffer[0], "0x%02X ", *(ptr)++);
					}

					// Send it to the ANE
					dispatchEvent( IN_LONGDATA, (const uint8_t*) buffer );
				}

				/* Queue the MIDIHDR for more input */
				midiInAddBuffer( hMidiIn, lpMIDIHeader, sizeof(MIDIHDR) );
			}

			break;

		case MIM_ERROR:
			dispatchEvent( IN_ERROR, (const uint8_t*) "MIM_ERROR" );
			break;

		case MIM_LONGERROR:
			dispatchEvent( IN_LONGERROR, (const uint8_t*) "MIM_LONGERROR" );
			break;

		case MIM_MOREDATA:
			// Used when midi data is coming too fast.
			dispatchEvent( IN_MOREDATA, (const uint8_t*) "MIM_MOREDATA" );
			break;

		default:
			dispatchEvent( IN_UNKNOWN, (const uint8_t*) "MIM_UNKNOWN" );
			break;
	}
	return;
}
//------------------------------------------------------------------------------
FREObject connectMidiIn(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]){
	FREObject result;
	FRENewObjectFromBool( 1, &result );

	uint32_t nMidiPort = 0;
	MMRESULT rv;

	unsigned long	err;

	FREGetObjectAsUint32( argv[0], &nMidiPort );

	rv = midiInOpen( &hMidiInDevice, nMidiPort, (DWORD)(void*)MidiInProc, 0, CALLBACK_FUNCTION );
	if(rv){
		outputMidiInErrorMsg( rv );

	}
	if (rv != MMSYSERR_NOERROR) {
		char str[1024];
		sprintf( str, "midiInOpen() failed...rv=%d", rv );
		dispatchEvent( IN_ERROR, (const uint8_t*) str );
		return result;
	}

	/* Store pointer to our input buffer for System Exclusive messages in MIDIHDR */
	//midiHdr.lpData = (LPBYTE)&SysXBuffer[0];
	midiHdr.lpData = (LPSTR)&SysXBuffer[0];

	/* Store its size in the MIDIHDR */
	midiHdr.dwBufferLength = sizeof(SysXBuffer);

	/* Flags must be set to 0 */
	midiHdr.dwFlags = 0;

	/* Prepare the buffer and MIDIHDR */
	err = midiInPrepareHeader(hMidiInDevice, &midiHdr, sizeof(MIDIHDR));
	if (!err){
		/* Queue MIDI input buffer */
		err = midiInAddBuffer(hMidiInDevice, &midiHdr, sizeof(MIDIHDR));
		if (!err){
			/* Start recording Midi */
			midiInStart(hMidiInDevice);

		}else{
			outputMidiInErrorMsg( err );

		}
	}else{
		outputMidiInErrorMsg( err );

	}

	return result;
}
//------------------------------------------------------------------------------
FREObject disconnectMidiIn(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]){
	FREObject result;
	FRENewObjectFromBool( 1, &result );

	/* We need to set a flag to tell our callback midiCallback()
	   not to do any more midiInAddBuffer(), because when we
	   call midiInReset() below, Windows will send a final
	   MIM_LONGDATA message to that callback. If we were to
	   allow midiCallback() to midiInAddBuffer() again, we'd
	   never get the driver to finish with our midiHdr
	*/
	SysXFlag |= 0x80;

	midiInReset(hMidiInDevice);

	midiInStop(  hMidiInDevice );
	midiInClose( hMidiInDevice );

	midiInUnprepareHeader( hMidiInDevice, &midiHdr, sizeof(MIDIHDR) );

	hMidiInDevice = NULL;

	return result;
}
//------------------------------------------------------------------------------
FREObject connectMidiOut(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]){
	FREObject result;
	UINT err;

	uint32_t nMidiPort = 0;
	FREGetObjectAsUint32( argv[0], &nMidiPort );

	err = midiOutOpen( &hMidiOutDevice, nMidiPort, 0, 0, CALLBACK_NULL );
	if(err){
		outputMidiOutErrorMsg( err );
		FRENewObjectFromBool( 0, &result );

	}else{
		FRENewObjectFromBool( 1, &result );

	}

	return result;
}
//------------------------------------------------------------------------------
FREObject disconnectMidiOut(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]){
	FREObject result;
	FRENewObjectFromBool( 1, &result );

	midiOutClose( hMidiOutDevice );
	hMidiOutDevice = NULL;

	return result;
}
//------------------------------------------------------------------------------
FREObject isSupported(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]){
	FREObject result;

	uint32_t isSupportedInThisOS = 1;
	FRENewObjectFromBool( isSupportedInThisOS, &result );

	return result;
}
//------------------------------------------------------------------------------
FREObject getInterfaceList(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]){
	FREObject result;

	UINT nMidiDeviceNum;
	MIDIINCAPS capsI;
	MIDIOUTCAPS capsO;
	unsigned int i;

	char str[1024];
	str[0] = '\0';

	// get number of INPUT interfaces
	nMidiDeviceNum = midiInGetNumDevs();
	for (i = 0; i < nMidiDeviceNum; ++i) {
		midiInGetDevCaps(i, &capsI, sizeof(MIDIINCAPS));
		strcat( str, "I "         ); // Lines starts with 'I' for INPUT
		strcat( str, capsI.szPname );
		strcat( str, "\n"         );
	}

	// get number of OUTPUT interfaces
	nMidiDeviceNum = midiOutGetNumDevs();
	for (i = 0; i < nMidiDeviceNum; ++i) {
		midiOutGetDevCaps(i, &capsO, sizeof(MIDIOUTCAPS));
		strcat( str, "O "         ); // Lines starts with 'O' for OUTPUT
		strcat( str, capsO.szPname );
		strcat( str, "\n"         );
	}

	// Stuff the string into the adobe object.
	FRENewObjectFromUTF8(strlen(str)+1, (const uint8_t *)str, &result);

	return result;
}
//------------------------------------------------------------------------------
FREObject transmitSysEx(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]){
	char str[1024];
	str[0] = '\0';

	FREObject result;

	UINT err;

	// Pull out the AS3 array.
	FREObject arr = argv[0]; // array
	uint32_t arr_len; // array length

	//showMessage( (const uint8_t*) "Inside transmitSysEx." );

	FREGetArrayLength(arr, &arr_len);


	sprintf(str, "Input array length: %d", arr_len );

	showMessage( (const uint8_t*)str );

	// create a C array for us to shove the values into
	unsigned char sysEx[8419];// = (unsigned char*)malloc(sizeof(unsigned char)*arr_len);

	showMessage( (const uint8_t*) "Looping over input." );
	// Loop over the AS3 array and shove the values into our C array that is used as the buffer.
	int32_t i = 0;
	for(i=0; i<arr_len; i++){
		// get an element at index
		FREObject element;
		FREGetArrayElementAt(arr, i, &element);

		// get an int value out of the element
		uint32_t value;
		FREGetObjectAsUint32(element, &value);

		// stuff it into our sysex array
		sysEx[i] = value;
	}

	sprintf(str, "Output array size: %d", sizeof(sysEx) );
	showMessage( (const uint8_t*)str );

	sprintf(str, "First element: %d", sysEx[0] );
	showMessage( (const uint8_t*)str );

	sprintf(str, "Last element: %d", sysEx[sizeof(sysEx)-1] );
	showMessage( (const uint8_t*)str );

	/* Store pointer in MIDIHDR */
	midiHdr.lpData = (LPSTR)&sysEx[0];

	/* Store its size in the MIDIHDR */
	midiHdr.dwBufferLength = sizeof(sysEx);

	/* Flags must be set to 0 */
	midiHdr.dwFlags = 0;

	showMessage( (const uint8_t*) "Preparing header to transmit." );
	/* Prepare the buffer and MIDIHDR */
	err = midiOutPrepareHeader( hMidiOutDevice,  &midiHdr, sizeof(MIDIHDR) );
	if (!err){
		/* Output the SysEx message. Note that this could return immediately if
		the device driver can output the message on its own in the background.
		Otherwise, the driver may make us wait in this call until the entire
		data is output
		*/
		showMessage( (const uint8_t*) "About to transmit the header." );
		err = midiOutLongMsg( hMidiOutDevice, &midiHdr, sizeof(MIDIHDR) );
		if (err){
			outputMidiOutErrorMsg( err );

		}else{
			showMessage( (const uint8_t*) "Send returned OK." );

		}

		/* Unprepare the buffer and MIDIHDR */
		while (MIDIERR_STILLPLAYING == midiOutUnprepareHeader( hMidiOutDevice, &midiHdr, sizeof(MIDIHDR)) ){
			/* Delay to give it time to finish */
			showMessage( (const uint8_t*) "In sleep, waiting for finish." );
			Sleep(1000);
		}
		showMessage( (const uint8_t*) "Transmission complete." );

	}else{
		outputMidiOutErrorMsg( err );
	}

	FRENewObjectFromBool( err == 0, &result );

	return result;
}
//------------------------------------------------------------------------------
FREObject transmitMessage(FREContext ctx, void* funcData, uint32_t argc, FREObject argv[]){
	FREObject result;
	UINT err;
	uint32_t dwMsg = 0;

	//showMessage( (const uint8_t*) "Inside transmitMessage." );

	FREGetObjectAsUint32( argv[0], &dwMsg );

	err = midiOutShortMsg( hMidiOutDevice, dwMsg );

	FRENewObjectFromBool( err == 0, &result );

	return result;
}
//------------------------------------------------------------------------------
void contextInitializer(void* extData, const uint8_t* ctxType, FREContext ctx, uint32_t* numFunctions, const FRENamedFunction** functions){
	*numFunctions = 8;
	FRENamedFunction* func = (FRENamedFunction*) malloc(sizeof(FRENamedFunction) * (*numFunctions));

	func[0].name = (const uint8_t*) "getInterfaceList";
	func[0].functionData = NULL;
	func[0].function = &getInterfaceList;

	func[1].name = (const uint8_t*) "isSupported";
	func[1].functionData = NULL;
	func[1].function = &isSupported;

	func[2].name = (const uint8_t*) "connectMidiIn";
	func[2].functionData = NULL;
	func[2].function = &connectMidiIn;

	func[3].name = (const uint8_t*) "connectMidiOut";
	func[3].functionData = NULL;
	func[3].function = &connectMidiOut;

	func[4].name = (const uint8_t*) "disconnectMidiIn";
	func[4].functionData = NULL;
	func[4].function = &disconnectMidiIn;

	func[5].name = (const uint8_t*) "disconnectMidiOut";
	func[5].functionData = NULL;
	func[5].function = &disconnectMidiOut;

	func[6].name = (const uint8_t*) "transmitSysEx";
	func[6].functionData = NULL;
	func[6].function = &transmitSysEx;

	func[7].name = (const uint8_t*) "transmitMessage";
	func[7].functionData = NULL;
	func[7].function = &transmitMessage;

	*functions = func;

	dllContext = ctx;
}
//------------------------------------------------------------------------------
void contextFinalizer(FREContext ctx){
	return;
}
//------------------------------------------------------------------------------
void initializer(void** extData, FREContextInitializer* ctxInitializer, FREContextFinalizer* ctxFinalizer){
	*ctxInitializer = &contextInitializer;
	*ctxFinalizer   = &contextFinalizer;
}
//------------------------------------------------------------------------------
void finalizer(void* extData){
	return;
}
//------------------------------------------------------------------------------
