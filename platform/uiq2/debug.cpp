
#include <e32svr.h> // RDebug
#include "debug.h"

#ifdef __WINS__

void ExceptionHandler(TExcType exc) {}

#else

static const wchar_t * const exception_names[] = {
	L"General",
	L"IntegerDivideByZero",
	L"SingleStep",
	L"BreakPoint",
	L"IntegerOverflow",
	L"BoundsCheck",
	L"InvalidOpCode",
	L"DoubleFault",
	L"StackFault",
	L"AccessViolation",
	L"PrivInstruction",
	L"Alignment",
	L"PageFault",
	L"FloatDenormal",
	L"FloatDivideByZero",
	L"FloatInexactResult",
	L"FloatInvalidOperation",
	L"FloatOverflow",
	L"FloatStackCheck",
	L"FloatUnderflow",
	L"Abort",
	L"Kill",
	L"DataAbort",
	L"CodeAbort",
	L"MaxNumber",
	L"InvalidVector",
	L"UserInterrupt",
	L"Unknown"
};


static void getASpace(TUint *code_start, TUint *code_end, TUint *stack_start, TUint *stack_end)
{
	TUint pc, sp;
	RChunk chunk;
	TFullName chunkname;
	TFindChunk findChunk(_L("*"));

	asm volatile ("str pc, %0" : "=m" (pc) );
	asm volatile ("str sp, %0" : "=m" (sp) );

	while( findChunk.Next(chunkname) != KErrNotFound ) {
		chunk.Open(findChunk);
		if((TUint)chunk.Base()+chunk.Bottom() < pc && pc < (TUint)chunk.Base()+chunk.Top()) {
			if(code_start) *code_start = (TUint)chunk.Base()+chunk.Bottom();
			if(code_end)   *code_end   = (TUint)chunk.Base()+chunk.Top();
		} else
		if((TUint)chunk.Base()+chunk.Bottom() < sp && sp < (TUint)chunk.Base()+chunk.Top()) {
			if(stack_start) *stack_start = (TUint)chunk.Base()+chunk.Bottom();
			if(stack_end)   *stack_end   = (TUint)chunk.Base()+chunk.Top();
		}
		chunk.Close();
	}
}

// tmp
#if defined(__DEBUG_PRINT)
extern "C" char *debugString();
#endif

// our very own exception handler
void ExceptionHandler(TExcType exc)
{
	TUint lr, sp, i;
	TUint stack_end	= 0;				// ending address of our stack chunk
	TUint code_start = 0, code_end = 0; // starting and ending addresses of our code chunk
	TUint guessed_address = 0;

	asm volatile ("str lr, %0" : "=m" (lr) );
	asm volatile ("str sp, %0" : "=m" (sp) );

	// first get some info about the chunks we live in
	getASpace(&code_start, &code_end, 0, &stack_end);

	// now we begin some black magic tricks
	// we go up our stack until we pass our caller address
	for(; sp < stack_end; sp += 4)
		if(*(TUint *)sp == lr) break;

	// there might be mirored caller address
	for(i = sp + 4; i < sp + 0x300 && i < stack_end; i += 4)
		if(*(TUint *)i == lr) { sp = i; break; }

	// aah, it is always 0x9c bytes away from the caller address in my firmware,
	// don't know how to detect it in any other way
	sp += 0x9c;
	guessed_address = *(TUint *)sp;

	// output the info
	TUint exec_show = exc;
	if(exec_show > 27) exec_show = 27;
	TPtrC ptrExc((TUint16 *) exception_names[exec_show]);

	RDebug::Print(_L("!!!Exception %i (%S) @ 0x%08x (guessed; relative=0x%08x)"), exc, &ptrExc, guessed_address, guessed_address - code_start);
#ifdef __DEBUG_PRINT_FILE
	DEBUGPRINT(   _L("!!!Exception %i (%S) @ 0x%08x (guessed; relative=0x%08x)"), exc, &ptrExc, guessed_address, guessed_address - code_start);
#endif

	TBuf<148> buff1;
	TBuf<10>  buff2;
	buff1.Copy(_L("  guessed stack: "));

	for(sp += 4, i = 0; i < 5 && sp < stack_end; sp += 4) {
		if((*(TUint *)sp >> 28) == 5) {
			if(i++) buff1.Append(_L(", "));
			buff2.Format(_L("0x%08x"), *(TUint *)sp);
			buff1.Append(buff2);
		}
		else if(code_start < *(TUint *)sp && *(TUint *)sp < code_end) {
			if(i++) buff1.Append(_L(", "));
			buff2.Format(_L("0x%08x"), *(TUint *)sp);
			buff1.Append(buff2);
			buff1.Append(_L(" ("));
			buff2.Format(_L("0x%08x"), *(TUint *)sp - code_start);
			buff1.Append(buff2);
			buff1.Append(_L(")"));
		}
	}
	RDebug::Print(_L("%S"), &buff1);
#ifdef __DEBUG_PRINT_FILE
	DEBUGPRINT(_L("%S"), &buff1);
#endif

	// tmp
#if defined(__DEBUG_PRINT)
	char *ps, *cstr = debugString();
	for(ps = cstr; *ps; ps++) {
	  if(*ps == '\n') {
	    *ps = 0;
	    dprintf(cstr);
		cstr = ps+1;
	  }
	}
#endif

//	RDebug::Print(_L("Stack dump:"));
//	asm volatile ("str sp, %0" : "=m" (sp) );
//	for(TUint i = sp+0x400; i >= sp-16; i-=4)
//		RDebug::Print(_L("%08x: %08x"), i, *(int *)i);

	// more descriptive replacement of "KERN-EXEC 3" panic
	buff1.Format(_L("K-EX3: %S"), &ptrExc);
	User::Panic(buff1, exc);
}

#endif // ifdef __WINS__


#if defined(__DEBUG_PRINT) || defined(__WINS__)

#ifndef __DLL__
	// c string dumper for RDebug::Print()
	static	TBuf<1024> sTextBuffer;
	TDesC* DO_CONV(const char* s)
	{
		TPtrC8	text8((TUint8*) (s));
		sTextBuffer.Copy(text8);
		return &sTextBuffer;
	}
#endif

#ifdef __DEBUG_PRINT_C
	#include <stdarg.h> // va_*
	#include <stdio.h>  // vsprintf

	// debug print from c code
	extern "C" void dprintf(char *format, ...)
	{
		va_list args;
		char    buffer[512];

		va_start(args,format);
		vsprintf(buffer,format,args);
		va_end(args);

		DEBUGPRINT(_L("%S"), DO_CONV(buffer));
	}
#endif

#ifdef __DEBUG_PRINT_FILE
	#include <f32file.h>

	// note: uses tls, leaks some mem
	void debugPrintFileInit()
	{
		RFs *fs = new(ELeave) RFs;
		fs->Connect();
		RFile *file = new(ELeave) RFile;
		// try to open
		TInt    res = file->Open(*fs, _L("C:\\documents\\Media files\\other\\snes9x.log"), EFileWrite|EFileShareAny);
		if(res) res = file->Open(*fs, _L("C:\\snes9x.log"), EFileWrite|EFileShareAny);
		if(!res) { TInt size; file->Size(size); file->Seek(ESeekStart, size); }
		// try to create
		if(res) res = file->Create(*fs, _L("C:\\documents\\Media files\\other\\snes9x.log"), EFileWrite|EFileShareAny);
		if(res) res = file->Create(*fs, _L("C:\\snes9x.log"), EFileWrite|EFileShareAny);
		Dll::SetTls(res ? 0 : file);
	}

	// debug print to file
	void debugPrintFile(TRefByValue<const TDesC> aFmt, ...)
	{
		// get RFile
		RFile *file = (RFile *) Dll::Tls();
		if(!file) return; // shit!

		TTime now; now.UniversalTime();
		TBuf<512>  tmpBuff;
		TBuf8<512> tmpBuff8;
		TInt size;

		file->Size(size); file->Seek(ESeekStart, size); // something else could have written to the file

		now.FormatL(tmpBuff, _L("%H:%T:%S.%C: "));
		tmpBuff8.Copy(tmpBuff);
		file->Write(tmpBuff8);

		VA_LIST args;
		VA_START(args, aFmt);
		tmpBuff.FormatList(aFmt, args);
		VA_END(args);
		tmpBuff8.Copy(tmpBuff);
		file->Write(tmpBuff8);

		file->Write(TPtrC8((TUint8 const *) "\r\n"));
		file->Flush();
	}
#endif

#endif

