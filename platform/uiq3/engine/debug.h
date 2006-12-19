#include <e32std.h>

#define __DEBUG_PRINT_C
#define __DEBUG_PRINT_FILE

#if defined(__DEBUG_PRINT) || defined(__WINS__)
	#include <e32svr.h> // RDebug
	#ifdef __DEBUG_PRINT_FILE
		void debugPrintFile(TRefByValue<const TDesC> aFmt, ...);
		#define DEBUGPRINT debugPrintFile
	#else
		#define DEBUGPRINT RDebug::Print
	#endif
	TDesC* DO_CONV(const char* s);
	#ifdef __DEBUG_PRINT_C
		#ifdef __cplusplus
		extern "C"
		#endif
		void dprintf(char *format, ...);
	#endif
#else
	#define DEBUGPRINT(x...)
	#undef __DEBUG_PRINT_C
	#undef __DEBUG_PRINT_FILE
#endif

void ExceptionHandler(TExcType exc);
