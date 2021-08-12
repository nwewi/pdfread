// this file contains global defines for testing that must be removed for final releases
#ifndef ___CCS_DEFINES_____
#define ___CCS_DEFINES_____

#ifndef WINVER 
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0601
#else
#undef _WIN32_IE
#define _WIN32_IE 0x0601
#endif

//#define ULLint int 
//#define ULLword unsigned int
//#define ULLdword DWORD 
//#define ULLlong long 
//#define ULLsize size_t 
#define ULLint ULONGLONG 
#define ULLword ULONGLONG
#define ULLdword ULONGLONG 
#define ULLlong ULONGLONG 
#define ULLsize ULONGLONG 

#define _OCR_SERVICE_
#define NOOCRONNWOCR

//#define __VIEW_REAL_IMAGE__
//#define _DBG_DONT_CHECK_USER_RIGHTS_
//#define __DEBUG_INDEX_VIEW__
//#define DEBUG_DOCREF

#define USE_VIRTUAL_PRINTER
#define __RETURN_ALL_TEXTBLOCKS__

//#define OWNBKColor

#define GEARV15
#define GEARV14  //if GEARV15 must be also on
#define GEARV15PDF

#ifdef NEWGEAR
#define GEARV15
#define GEARV14  //if GEARV15 must be also on
#define GEARV15PDF
#endif

#ifdef _DEBUG
#define GEARV15
#ifndef GEARV15PDF
#define GEARV15PDF
#endif
#endif
#define GEARV14  //if GEARV15 must be also on

#ifndef ALLWAYSDIRECTIMPORT
#ifndef GEARV15PDF
#define GEARV15PDF
#endif
#else
#define DIRECTIMPORT
#endif

#ifndef DIRECTIMPORT
#ifndef GEARV15PDF
#define GEARV15PDF
#endif
#endif

#ifdef _DEBUG
//#define LICCNT
//#define LICWITHCOMP
#endif
//#define LICFILE
//#define LICFILEBOMB

//#define _SPEED_DEBUG
#ifdef _DEBUG_RULE
#define _TRACE_RULES
#define _TRACE_RULES_DUMP
#define _DEBUG_RULE_TRACE
#define RULESEDIT
#endif
//
//#define _TRACE_CMD
//#define _TRACE_OBJ
//#define _REPORT_NOT_LINKED_

//#define PUSHPOP_OBJECTS_DEBUG
//#define _MORE_DEBUG_LINES_ 10000

//#define _DEVELOP_RULES_MENU_

//#define USETK
//#define USEDYNALIB

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

#define _CRT_SECURE_NO_DEPRECATE

#pragma warning ( disable : 6031 )

#endif

