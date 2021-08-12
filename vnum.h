/*! \addtogroup main
 \{ */
//! \file vnum.h \brief This file contains versioning information on the current build and needs to be changed for each new build
#define __LQUOTE( x )			L#x
#define __INDIRECT_LQUOTE( x )	__LQUOTE( x )
#define __AQUOTE( x )			#x
#define __INDIRECT_AQUOTE( x )	__AQUOTE( x )
#define __LSTR( x )			_T(x)

#include "SUBNUM.h"
#define BUILDNUM 0
#define SRCNUM 7.1

#define VNUMNC __INDIRECT_LQUOTE(SRCNUM)
#define VNUMPC L"4.2"
#define VNUMDW L"4.3"
#define VNUMNCW L"7.1"

#ifndef CCS_BUILD_YEAR
#define CCS_BUILD_YEAR 2020
#endif
#define CCS_BUILD_YEAR_STRING __INDIRECT_AQUOTE(CCS_BUILD_YEAR)
#define CCS_BUILD_YEAR_STRINGW __INDIRECT_LQUOTE(CCS_BUILD_YEAR)

#define FVERSION __INDIRECT_LQUOTE(SRCNUM.BUILDNUM.SUBNUM)
#define FVERSIONDISPLAY __INDIRECT_LQUOTE(SRCNUM-BUILDNUM) L"(#" __INDIRECT_LQUOTE(SUBNUM) L")\0"
#define FVERSIONNUM SRCNUM.BUILDNUM.SUBNUM
#define COPYRIGHTA "Copyright © 1997-" CCS_BUILD_YEAR_STRING " newsWorks GmbH & Co. KG, Hamburg, Germany\0"
#define COPYRIGHT L"Copyright © 1997-" CCS_BUILD_YEAR_STRINGW L" newsWorks GmbH & Co. KG, Hamburg, Germany\0"
#define COPYRIGHTSHORTA "© 1997-"  CCS_BUILD_YEAR  " newsWorks GmbH & Co. KG, Hamburg, Germany\0"
#define COPYRIGHTSHORT L"© 1997-"  CCS_BUILD_YEAR_STRINGW  L" newsWorks GmbH & Co. KG, Hamburg, Germany\0"
#define COPYRIGHT_LONG L"Copyright © 1997-" CCS_BUILD_YEAR_STRINGW L" newsWorks GmbH & Co. KG, Hamburg, Germany\0"
#define COMPANY L"newsWorks GmbH & Co. KG, Hamburg, Germany\0"
#define PRODUCTNAME L"newsWorks\0"
#define PRODVERSION L"7,1," __INDIRECT_LQUOTE(BUILDNUM) L"," __INDIRECT_LQUOTE(SUBNUM\0)
#define FVERSIONWORD 7,1,BUILDNUM,SUBNUM
#define PRODVERSIONWORD 7,1,BUILDNUM,SUBNUM
//#define PGMSUPPORT "+ 49 40 - 227 130 19\0"
//#define PGMWEB L"http://www.newsworks.de\0"
#define PGMWEB L"https://extranet.newsWorks.de/nW/SitePages/Home.aspx\0"
#define PGMSUPPORT L"+49 40 227130-96\0"
//#define PGMSUPPORT "0190 1 45026\0"
#define VERINCOUNTER __DATE__ __TIME__

#define NEEDED_CDKEY CFG_VERSION_71
/*! \} */
