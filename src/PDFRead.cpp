// PDFRead.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include <winerror.h>
#include "PDFRead.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//#define addlog

// The one and only application object

//===========================================================================
CWinApp theApp;

using namespace std;

extern int PdfWork(int argc, TCHAR* argv[]);

//===========================================================================
int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

  //setvbuf(stdout,NULL,_IONBF,0);
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

  //_tprintf(_T("    main programm\r\n"));

  //ASSERT(FALSE);

	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0)) {
		_tprintf(_T("!!! Fatal Error: MFC initialization of PDFRead failed\r\n"));
		nRetCode = 2001;
  } else {
        nRetCode = PdfWork(argc,argv);
    }
    return nRetCode;
}
