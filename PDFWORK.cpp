#include "stdafx.h"

#include "../../vnum.h"

extern "C" {
#include "../../mupdf/include/mupdf/fitz.h"
}

#include ".\libtiff\libtiff\tiffiop.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

extern "C" int isRecursive;

//#define addlog
#define DIV1 1024
#define DIV2 DIV1*DIV1

//#define TRACEMEM
#ifdef TRACEMEM
static int memtrace_current = 0;
static int memtrace_peak = 0;
static int memtrace_total = 0;
static int memID = 0;
CPtrArray Allptr;

static void *
trace_malloc(void *arg, unsigned int size)
{
	int *p;
	if (size == 0)
		return NULL;
	p = (int*)malloc(size + (sizeof(unsigned int)*2));
	if (p == NULL)
		return NULL;
	p[0] = size;
    p[1] = memID++;
	memtrace_current += size;
	memtrace_total += size;
	if (memtrace_current > memtrace_peak)
		memtrace_peak = memtrace_current;
    Allptr.Add(p);
	return (void *)&p[2];
}

static void
trace_free(void *arg, void *p_)
{
	int *p = (int *)p_;

	if (p == NULL)
		return;
    p = &p[-2];
	memtrace_current -= p[0];
    for (int pi = Allptr.GetCount() - 1; pi >= 0; pi--)
        if (Allptr[pi] == p)
            Allptr.RemoveAt(pi);
	free(p);
}

static void *
trace_realloc(void *arg, void *p_, unsigned int size)
{
	int *p = (int *)p_;
	unsigned int oldsize;

	if (size == 0)
	{
		trace_free(arg, p_);
		return NULL;
	}
	if (p == NULL)
		return trace_malloc(arg, size);
    p = &p[-2];
    oldsize = p[0];
    for (int pi = Allptr.GetCount() - 1; pi >= 0; pi--)
        if (Allptr[pi] == p)
            Allptr.RemoveAt(pi);
	p = (int*)realloc(p, size + (sizeof(unsigned int)*2));
    if (p == NULL) {
        return NULL;
    }
	memtrace_current += size - oldsize;
	if (size > oldsize)
		memtrace_total += size - oldsize;
	if (memtrace_current > memtrace_peak)
		memtrace_peak = memtrace_current;
	p[0] = size;
    p[1] = memID++;
    Allptr.Add(p);
	return &p[2];
}

void dumpptr(int fromid)
{
    for (int pi = 0; pi <  Allptr.GetCount(); pi++) {
        int* p = (int*)(Allptr[pi]);
        if (p[1] >= fromid) {
            TRACE1("mem %d\r\n", p[1]);
        }
    }
}
#endif
//===========================================================================*/
CStringA ToUtf8(CStringW instr)
{
    CStringA ResdataA;
    LPSTR ResdataAp = ResdataA.GetBuffer(instr.GetLength() * 4);
    int newlen = WideCharToMultiByte(CP_UTF8, 0, instr, instr.GetLength(), ResdataAp, instr.GetLength() * 4, NULL, NULL);
    ResdataA.ReleaseBuffer(newlen);
    return ResdataA;
}
//==========================================================
CString GetPageFileName(int pg)
{
    CString res;
    res.Format(_T("_%04d"), pg);
    return res;
}
//==========================================================
bool WriteStringToFile(const CString& filepath, CStringA content)
{
    try {
        CFile wrfile;
        wrfile.Open(filepath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary);
        wrfile.Write((LPCSTR)content, content.GetLength());
        wrfile.Close();
    }
    catch (...) {
        return false;
    }
    return true;
}
//==========================================================
CString AbortFileName;
CString CurPDFFile;
CString CurOutPath;
CStringArray ExtFiles;
bool WithProgressMsg = false;
//===========================================================================
int isRecursive=0;
int AllPDFMinWidth = 0;
//===========================================================================
bool bMakeDetailLog = false;
CString DetailLogFileName;
int DefaultRes = 300;
bool skipdefimage = false;
int MainLanguage =0;
bool withpdftext = true;

void SendDetails(CString msg);

void AddDetailsLog(const char * _fmt,...)
{
    if (bMakeDetailLog) {
        CStringA allmsg;
        va_list argptr;
        va_start(argptr, _fmt);
        allmsg.FormatV(_fmt,argptr);
        va_end(argptr);
        SendDetails(CString(allmsg));
    }
}

void InitDatailLog()
{
    DetailLogFileName.Format(_T("%s_pdfimport.log"), CurOutPath);
    CStdioFile logfile(DetailLogFileName,CFile::modeCreate);
    logfile.Close();
    addCCSLog=AddDetailsLog;
    bMakeDetailLog=true;
}
void SendDetails(CString msg)
{
    if (bMakeDetailLog) {
        try {
            CStdioFile logfile(DetailLogFileName,CFile::modeCreate|CFile::modeNoTruncate|CFile::modeWrite|CFile::shareDenyWrite);
            logfile.SeekToEnd();
            logfile.WriteString(_T("->")+msg+_T("\r\n"));
            logfile.Close();
        }
        catch(...) {
        }
    }
    if (!WithProgressMsg)
        _tprintf(_T("."));
}
void SendDetailsMsg(CString format,...)
{
    CString msg;
	va_list argList;
	va_start( argList, format );
	msg.FormatV(format,argList);
	va_end( argList );
    SendDetails(msg);
}
//==========================================================
//==========================================================
//===========================================================================
void SendErrorMsg(LPCTSTR msg)
{
     _tprintf(_T("\r\n!!! Report Error: %s on pdf file %s\r\n"),(LPCTSTR)msg,(LPCTSTR)CurPDFFile);
}
//---------------------------------------------------------------------------
int MessageRedirector(LPCTSTR lpszText, UINT nType)
{
    _tprintf(_T("\r\n!!! Error Message: %s  on pdf file %s\r\n"),lpszText,(LPCTSTR)CurPDFFile);

    if (nType & MB_ABORTRETRYIGNORE)
        return IDABORT;
    if ((nType & MB_OKCANCEL)|(nType & MB_RETRYCANCEL )|(nType & MB_YESNOCANCEL)) 
        return IDCANCEL;
#if(WINVER >= 0x0500)
    if (nType & MB_CANCELTRYCONTINUE) 
        return IDCANCEL;
#endif
    if (nType & MB_YESNO) 
        return IDNO;
    if (nType & MB_OK) 
        return IDOK;

    return IDCLOSE;
}
void SendImageName(int typ,int pgnum, const CString path)
{
    _tprintf(_T("\r\n&&&%d %d %s\r\n"), typ, pgnum, (LPCTSTR)path);
}
void SendPageStart(int pg, int all)
{
    _tprintf(_T("\r\n>>>%d %d"),pg,all);
}
void SendPageEnd(int pg, int all)
{
    _tprintf(_T("\r\n<<<%d %d"), pg, all);
}
//===========================================================================
static int prevprozent = -1;
static int nextprogressstart = 0;
static int maxprogresssteps = 6;

bool SubProgressSend(LPVOID data,int prozprogress)
{
    if (prevprozent != prozprogress) {
        if (WithProgressMsg) {
            int showprogress = ((nextprogressstart * 100) + prozprogress) / maxprogresssteps;
            if ((prozprogress > 0) || (prevprozent < 0))
                _tprintf(_T("\r\n+++%d \r\n"), showprogress);
        } else
            _tprintf(_T("."));
    } else
        if ((prozprogress&0x1f)==0)
            _tprintf(_T("."));
    prevprozent = prozprogress;
    return false; // no abort
}
//===========================================================================
void ShowCookie(fz_cookie* cookie)
{
    if (cookie)
        if (cookie->progress_max <= 0)
            SubProgressSend(cookie, cookie->progress / 10); // 100% =1000
        else
            SubProgressSend(cookie, (cookie->progress * 100) / cookie->progress_max);
}
//===========================================================================
bool ScriptAliveCallBack();
void ShowIntAlive()
{
    ScriptAliveCallBack();
}
//===========================================================================
void SetProgress(int cur, int max)
{
    //  if (WithProgressMsg)
    _tprintf(_T("\r\n--- %d %d \r\n"),cur,max);
}
//===========================================================================
void ModChangeSend(CString mod)
{
    //  if (WithProgressMsg)
    //_tprintf(_T("*** %s \r\n"),mod);
    if (WithProgressMsg) 
        _tprintf(_T("\r\n  %s"), (LPCWSTR)mod);
    else
        _tprintf(_T("."));
}
/////////////////////////////////////////////////////////////////////////////
int ScriptActCount = 0;
//----------------------------------------------------------
bool ScriptAliveCallBack()
{
    if ((ScriptActCount++ & 0x1f)==0)
        _tprintf(_T("."));
    return false;  // do not break
}
/////////////////////////////////////////////////////////////////////////////
static void
PutWin32Error(const char* modulestr, const char* fmt, va_list ap)
{
    CString msg;
    CStringA msg1;
    DWORD enumber = GetLastError();
    vsprintf(msg1.GetBuffer(10000), fmt, ap);
    msg1.ReleaseBuffer();
    if (modulestr != NULL)
        msg.Format(_T("%s: %d %s."),CString(modulestr),enumber,CString(msg1));
    else 
        msg.Format(_T("%d %s."), enumber, CString(msg1));
    SendErrorMsg(msg);
}
/////////////////////////////////////////////////////////////////////////////
double RecalcPageSize(fz_rect& pagesize)
{
    if (AllPDFMinWidth > 10) {
        double calcsize = (AllPDFMinWidth*72.0) / 254;
        float w = abs(pagesize.x1 - pagesize.x0);
        float h = abs(pagesize.y1 - pagesize.y0);
        if (w < h) {
            if ((w != 0) && (((w - calcsize) + 1) < 0)) {
                return (calcsize / w);
            } else
                return 1;
        } else {
            if ((h != 0) && (((h - calcsize) + 1) < 0)) {
                return (calcsize / h);
            } else
                return 1;
        }
    } else
        return 1;
}
/////////////////////////////////////////////////////////////////////////////
enum { PIT_TYPE_ORIGINAL, PIT_TYPE_IMAGEONLY, PIT_TYPE_PATHONLY, PIT_TYPE_IMAGEANDPATH, PIT_TYPE_TEXTONLY, PIT_TYPE_ALL };
static const TCHAR* page_image_type_sufix[PIT_TYPE_ALL] =
{
    _T("")
    ,_T("_img")
    , _T("_path")
    , _T("_impath")
    , _T("_text")

};

void RenderPage(int pageimagetype, int threshold, bool& m_WasError, fz_context *ctx, int pgnum, int imgres, bool skipdefimage, fz_page *fzpage, fz_display_list *fzlist, fz_cookie& cookie, fz_colorspace *colorspace)
//=================================================================================
{
    SendDetailsMsg(_T("generate image page %d"), pgnum + 1);
    fz_rect pagesize;
    fz_matrix ctm;
    fz_irect ibounds;
    fz_device *dev = NULL;
    fz_pixmap *pix = NULL;

    fz_try(ctx) {
        pagesize = fz_bound_page(ctx, fzpage);

        float rotation = 0;
        float zoom = (RecalcPageSize(pagesize) * (imgres / 72.0 / 2)); //Twice as small

        ctm = fz_pre_scale(fz_rotate(rotation), zoom, zoom);
        fz_rect tbounds = pagesize;

        ibounds = fz_round_rect(fz_transform_rect(tbounds, ctm));
        //ibounds = fz_round_rect(tbounds);
        tbounds = fz_rect_from_irect(ibounds);

        pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds,NULL,1);
        fz_set_pixmap_resolution(ctx, pix, imgres / 2, imgres / 2);
        fz_clear_pixmap_with_value(ctx, pix, 255);

        if (PIT_TYPE_IMAGEONLY == pageimagetype)
            dev = fz_new_imagesonly_device(ctx, fz_identity, pix);
        else if (PIT_TYPE_PATHONLY == pageimagetype)
            dev = fz_new_pathsonly_device(ctx, fz_identity, pix);
        else if (PIT_TYPE_IMAGEANDPATH == pageimagetype)
            dev = fz_new_imagesandpathsonly_device(ctx, fz_identity, pix);
        else if (PIT_TYPE_TEXTONLY == pageimagetype)
            dev = fz_new_textsonly_device(ctx, fz_identity, pix);

        fz_enable_device_hints(ctx, dev, FZ_DONT_INTERPOLATE_IMAGES);
        if (skipdefimage)
            fz_enable_device_hints(ctx, dev, FZ_CCS_SKIP_INVALID);
        //						fz_run_page(fzdoc, fzpage, dev, &ctm, &cookie);
        fz_run_display_list(ctx, fzlist, dev, ctm, tbounds, &cookie);
        fz_close_device(ctx, dev);
        fz_drop_device(ctx, dev);
        dev = NULL;

        ModChangeSend(_T("image generated"));
        SendDetailsMsg(_T("move image page %d"), pgnum + 1);

        SendDetailsMsg(_T("generated image is x=%d y=%d w=%d h=%d n=%d"), pix->x, pix->y, pix->w, pix->h, pix->n);
        nextprogressstart++;


        TIFF * tif = NULL;
        CString output_file = CurOutPath + GetPageFileName(pgnum) + page_image_type_sufix[pageimagetype] + _T(".tif");

        fz_try(ctx) {

            if ((tif = TIFFOpen(ToUtf8(output_file), "w")) == NULL) {
                CString msg;
                msg.Format(_T("File error on page %d in file '%s'\r\n"), pgnum, CurPDFFile);
                SendErrorMsg(msg);
                fz_throw(ctx,2, ToUtf8(msg));
            }

            TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, pix->w);
            TIFFSetField(tif, TIFFTAG_IMAGELENGTH, pix->h);
            TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
            TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
            TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
            TIFFSetField(tif, TIFFTAG_XRESOLUTION, (double)(imgres/2));
            TIFFSetField(tif, TIFFTAG_YRESOLUTION, (double)(imgres / 2));
            TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);

            if (0 != threshold) {
                TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 1);
                TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_CCITTFAX4);
                TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
                TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
            } else {
                TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
                TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
                TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
                TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
            }
        }
        fz_catch(ctx)
        {
            CString msg, msgdata;
            CString TestPath;
            MEMORYSTATUSEX memstate;
            unsigned __int64 i64FreeBytesToCaller, i64TotalTypes, i64FreeBytes;
            msg.Format(_T("tiff create error on page %d in file '%s'"), pgnum, CurPDFFile);
            memset(&memstate, 0, sizeof(memstate));
            memstate.dwLength = sizeof(memstate);
            GetTempPath(_MAX_PATH, TestPath.GetBuffer(_MAX_PATH + 10));
            TestPath.ReleaseBuffer();
            GetDiskFreeSpaceEx(TestPath, (PULARGE_INTEGER)&i64FreeBytesToCaller, (PULARGE_INTEGER)&i64TotalTypes, (PULARGE_INTEGER)&i64FreeBytes);
            GlobalMemoryStatusEx(&memstate);
            msgdata.Format(_T("\r\nmemory status is %I32x %d%% (%I64d/%I64d/%I64d/%I64d/%I64d/%I64d)\r\nTemp is %s : (%I64d/%I64d/%I64d) : %d"),
                ::GetVersion(), memstate.dwMemoryLoad, memstate.ullTotalPhys / DIV1, memstate.ullAvailPhys / DIV1, memstate.ullTotalPageFile / DIV1, memstate.ullAvailPageFile / DIV1, memstate.ullTotalVirtual / DIV1, memstate.ullAvailVirtual / DIV1,
                TestPath, i64FreeBytesToCaller / DIV2, i64TotalTypes / DIV2, i64FreeBytes / DIV2, _taccess(TestPath, 06));
            SendErrorMsg(msg+msgdata);
            SendDetails(msg + msgdata);
            m_WasError = true;
            fz_rethrow(ctx);
        }


        fz_try(ctx) {
            unsigned char *p = pix->samples;

            if (0 != threshold) {
                unsigned char *o = new unsigned char[pix->w];
                
                int x, y, xl, xx;

                for (y = 0; y < pix->h; y++) {
                    xl = 0;

                    for (x = 0; x < pix->w; x = x + 8){
                        o[xl] = 0;
                        for (xx = x; xx < (x + 8) && xx < pix->w; xx++){
                            int px = 0;
                            int bx = 0;

                            switch (pix->n){
                            case 2: px = p[0]; break;
                            case 4: px = (0.2126F * p[0] + 0.7152F * p[1] + 0.0722F * p[2]); break;
                            }

                            bx = px < threshold ? 0 : 1;
                            o[xl] = (o[xl] << 1) | bx;
                            p += 4;
                        }
                        xl++;
                    }
                    TIFFWriteScanline(tif, o, y, 0);

                }
                delete[] o;
            }
            else {
                unsigned char *ln = new unsigned char[pix->w*3];
                for (int i = 0; i < pix->h; i++) {
                    int xl = 0;
                    ScriptAliveCallBack();
                    for (int b = 0; b < pix->w; ++b)
                    {
                        ln[xl++] = p[0];
                        ln[xl++] = p[1];
                        ln[xl++] = p[2];
                        p += 4;
                    }
                    TIFFWriteScanline(tif, ln, i, 0);
                }
                delete[] ln;
            }
            p = NULL;
        }
        fz_catch(ctx)
        {
            CString msg;
            msg.Format(_T("pixel format error on page %d in file '%s'"), pgnum, CurPDFFile);
            SendErrorMsg(msg);
            SendDetails(msg);
            m_WasError = true;
            fz_rethrow(ctx);
        }
        fz_try(ctx) {
            TIFFClose(tif);
        }
        fz_catch(ctx)
        {
            CString msg, msgdata;
            CString TestPath;
            MEMORYSTATUSEX memstate;
            unsigned __int64 i64FreeBytesToCaller, i64TotalTypes, i64FreeBytes;
            msg.Format(_T("image generation error on page %d in file '%s'"), pgnum, CurPDFFile);
            memset(&memstate, 0, sizeof(memstate));
            memstate.dwLength = sizeof(memstate);
            GetTempPath(_MAX_PATH, TestPath.GetBuffer(_MAX_PATH + 10));
            TestPath.ReleaseBuffer();
            GetDiskFreeSpaceEx(TestPath, (PULARGE_INTEGER)&i64FreeBytesToCaller, (PULARGE_INTEGER)&i64TotalTypes, (PULARGE_INTEGER)&i64FreeBytes);
            GlobalMemoryStatusEx(&memstate);
            msgdata.Format(_T("\r\nmemory status is %I32x %d%% (%I64d/%I64d/%I64d/%I64d/%I64d/%I64d)\r\nTemp is %s : (%I64d/%I64d/%I64d) : %d"),
                ::GetVersion(), memstate.dwMemoryLoad, memstate.ullTotalPhys / DIV1, memstate.ullAvailPhys / DIV1, memstate.ullTotalPageFile / DIV1, memstate.ullAvailPageFile / DIV1, memstate.ullTotalVirtual / DIV1, memstate.ullAvailVirtual / DIV1,
                TestPath, i64FreeBytesToCaller / DIV2, i64TotalTypes / DIV2, i64FreeBytes / DIV2, _taccess(TestPath, 06));
            SendErrorMsg(msg+ msgdata);
            SendDetails(msg + msgdata);
            m_WasError = true;
            fz_rethrow(ctx);
        }
        ModChangeSend(_T("image moved"));

        SendImageName(pageimagetype,pgnum,output_file);
        SendDetailsMsg(_T("save image page %d to %s"), pgnum + 1, output_file);

        ExtFiles.Add(output_file);

    }
    fz_always(ctx)
    {
        if (pix != NULL)
        {
            fz_clear_pixmap(ctx, pix);
            fz_drop_pixmap(ctx, pix);
        }
        pix = NULL;

    }
    fz_catch(ctx) {
        fz_drop_page(ctx, fzpage);
        fzpage = NULL;
        fz_rethrow(ctx);
    }

    ModChangeSend(_T("image done"));
}
//===========================================================================*/
//===========================================================================*/
void AddRectToDoc(LPVOID pvPage, int typeofrect, int left, int top, int right, int bottom)
{
    _tprintf(_T("\r\n===%d,%d,%d,%d\r\n"),left, top, right, bottom);
}
//===========================================================================*/
bool isnwstr(CStringA& tstr)
{
    LPCSTR testdef[] = { "newsworks", "newspress", "newsflow", "newsclip", "procserv", 0 };
    int p = 0;
    while (testdef[p]) {
        if (tstr.Find(testdef[p]) >= 0) return true;
        p++;
    }
    return false;
}
//===========================================================================*/
int LoadPdfFile(CString ExcludeString)
{
    bool m_WasError = false;
    bool isAbort = false;
    fz_document *fzdoc = NULL;
#ifdef TRACEMEM
	fz_alloc_context alloc_ctx = { NULL, trace_malloc, trace_realloc, trace_free };
    fz_context *ctx = fz_new_context(&alloc_ctx,NULL, FZ_STORE_DEFAULT);
#else
    fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT); // for WIN64 -> , FZ_STORE_UNLIMITED);
#endif
    char *password = ""; //Password protecteded PDF not supported

    if (ctx == NULL) {
        CString msg;
        msg.Format(_T("memory allocation error: %s"), CurPDFFile);
        SendErrorMsg(msg);
        m_WasError=true;
        return 1;
    }

    UINT imgres = DefaultRes;

    fz_register_document_handlers(ctx);

    fz_set_aa_level(ctx, 1);

    CStringA filename(ToUtf8(CurPDFFile));

    fz_try(ctx) {
        SendDetailsMsg(_T("Open pdf doc %ws"), CurPDFFile);
        fzdoc = fz_open_document(ctx, filename);
    }
    fz_catch(ctx) {
        CString msg;
        msg.Format(_T("cannot open document: %ws"), CurPDFFile);
        SendErrorMsg(msg);
        SendDetails(msg);
        m_WasError=true;
        fz_drop_context(ctx);
        return 7;
    }
    if (!m_WasError && fz_needs_password(ctx, fzdoc)) {
        if (!fz_authenticate_password(ctx,fzdoc, password)) {
            CString msg;
            msg.Format(_T("Import PDF error, Document protected, authorization failed for %ws"), CurPDFFile);
            SendErrorMsg(msg);
            SendDetails(msg);
            m_WasError=true;
            fz_drop_document(ctx, fzdoc);
            fz_drop_context(ctx);
            return 8;
        }
    }

    DWORD allpganz = fz_count_pages(ctx, fzdoc);

    if (bMakeDetailLog)
        maxprogresssteps++;

    fz_colorspace *colorspace = fz_device_rgb(ctx);

    //Main loop through the pages==============================================================
    if (allpganz > 0) {
        bool isNwReimport = false;
        {
#define RESBUFSIZE 4096
            char resbuf[RESBUFSIZE];
            try {
                int reslen = fz_lookup_metadata(ctx, fzdoc, "info:Producer", resbuf, RESBUFSIZE);
                if (reslen > 0) {
                    resbuf[reslen] = 0;
                    CStringA TestStr(resbuf);
                    TestStr = TestStr.MakeLower();
                    if (isnwstr(TestStr))
                        isNwReimport = true;
                }
                if (!isNwReimport) {
                    reslen = fz_lookup_metadata(ctx, fzdoc, "info:Creator", resbuf, RESBUFSIZE);
                    if (reslen > 0) {
                        resbuf[reslen] = 0;
                        CStringA TestStr(resbuf);
                        TestStr = TestStr.MakeLower();
                        if (isnwstr(TestStr))
                            isNwReimport = true;
                    }
                }
                if (!isNwReimport) {
                    // special case newsclip print
                    reslen = fz_lookup_metadata(ctx, fzdoc, "info:Title", resbuf, RESBUFSIZE);
                    if (reslen > 0) {
                        resbuf[reslen] = 0;
                        CStringA TestStr(resbuf);
                        TestStr = TestStr.Trim();
                        if (TestStr.Compare("newsWorks Clip")==0)
                            isNwReimport = true;
                    }
                }
            }
            catch (...) {};
            }

#ifdef TRACEMEM        
        int startid = memID;
#endif

        for(DWORD pgnum=0;(!m_WasError) && (pgnum < allpganz); pgnum++) {
#ifdef MAKEDUMP
            _CrtMemState s1, s2, s3;    
            _CrtMemCheckpoint(&s1);
            {
#endif
                SendDetailsMsg(_T("starting page %d"), pgnum + 1);
                SendPageStart(pgnum,allpganz);
                CString tif_output_file = CurOutPath + GetPageFileName(pgnum) + _T(".tif");
                UINT state = 0;
                nextprogressstart = 0;
                isRecursive = 0;
                SetProgress(pgnum, allpganz);
                if ((_taccess(AbortFileName, 0) == 0)) {
                    SendErrorMsg(_T("Import aborted from main programm"));
                    SendDetails(_T("Import aborted from main programm"));
                    m_WasError = true;
                    isAbort = true;
                    break;
                }
                CString pgsearch; pgsearch.Format(_T(",%d,"),pgnum+1);
                if (ExcludeString.Find(pgsearch) >= 0) {
                    
                    continue;
                }
                try {
                    //Variables===================================================================================
                    fz_page *fzpage;
                    fz_display_list *fzlist = NULL;
                    fz_device *dev = NULL;
                    fz_device *testdev = NULL;
                    fz_pixmap *pix = NULL;
                    fz_cookie cookie = { 0 };
                    fz_matrix ctm;
                    fz_rect pagesize;
                    fz_rect tbounds;
                    fz_irect ibounds;
                    int is_color = 0;

                    float zoom;
                    //int w,h;

                    fz_var(dev);
                    fz_var(pix);

                    SendDetailsMsg(_T("loading page %d"), pgnum + 1);
                    //Load Page====================================================================================
                    state++;
                    fz_try(ctx)
                    {
                        fzpage = fz_load_page(ctx, fzdoc, pgnum);
                    }
                    fz_catch(ctx)
                    {
                        CString msg;
                        msg.Format(_T("cannot load page %d in file '%ws'"), pgnum, CurPDFFile);
                        SendErrorMsg(msg);
                        SendDetails(msg);
                        m_WasError = true;
                        fz_drop_page(ctx, fzpage);
                        fzpage = NULL;
                        break;
                    }
                    if (fzpage == NULL) {
                        CString msg;
                        msg.Format(_T("Import PDF error Could not create Image for %ws"), CurPDFFile);
                        SendErrorMsg(msg);
                        SendDetails(msg);
                        m_WasError = true;
                        //fz_drop_page(fzdoc,fzpage); fzpage allready empty
                        fzpage = NULL;
                        break;
#ifdef _DEBUG
                    } else { //QUESTION--> convert to BW and check the blackness
#endif
                    }
                    state++;
                    ModChangeSend(_T("loaded"));
                    SendDetailsMsg(_T("calc page %d"), pgnum + 1);
                    fz_rect bounds;
                    bounds = fz_bound_page(ctx, fzpage);


                    SendDetailsMsg(_T("parse page %d"), pgnum + 1);

                    fz_try(ctx)
                    {
                        fzlist = fz_new_display_list(ctx, bounds);
                        dev = fz_new_list_device(ctx, fzlist);
                        if (skipdefimage)
                            fz_enable_device_hints(ctx, dev, FZ_CCS_SKIP_INVALID);
                        testdev = fz_new_test_device(ctx, &is_color, 0.01f, 0, dev);
                        fz_run_page(ctx, fzpage, testdev, fz_identity, &cookie);
                        fz_close_device(ctx, testdev);
                        // old fz_run_page(ctx, fzpage, dev, &fz_identity, &cookie);
                    }
                    fz_always(ctx)
                    {
                        fz_drop_device(ctx, testdev);
                        testdev = NULL;
                        fz_drop_device(ctx, dev);
                        dev = NULL;
                    }
                    fz_catch(ctx)
                    {
                        CString msg;
                        msg.Format(_T("cannot analyse page %d in file '%ws'"), pgnum, CurPDFFile);
                        SendErrorMsg( msg);
                        SendDetails(msg);
                        m_WasError = true;
                        if (fzlist)
                            fz_drop_display_list(ctx, fzlist);
                        fz_drop_page(ctx, fzpage);
                        fzpage = NULL;
                        fzlist = NULL;
                        break;
                    }
                    ModChangeSend(_T("list done"));

                    //if (bMakeDetailLog) {
                    //    SendDetailsMsg(_T("generate dump page %d"), pgnum + 1);

                    //    fz_buffer *buf = NULL;
                    //    fz_output *out = NULL;
                    //    fz_try(ctx)
                    //    {
                    //        buf = fz_new_buffer(ctx, 1024); //fz_buffer grows automatically (50% more space when needed)
                    //        out = fz_new_output_with_buffer(ctx, buf);

                    //        dev = fz_new_tracebuf_device(ctx, out);

                    //        if (skipdefimage)
                    //            fz_enable_device_hints(ctx, dev, FZ_CCS_SKIP_INVALID);
                    //        fz_run_display_list(ctx, fzlist, dev, &fz_identity, &fz_infinite_rect, &cookie);

                    //        SendDetails(CString((LPCSTR)(fz_string_from_buffer(ctx,buf))));
                    //        nextprogressstart++;
                    //    }
                    //    fz_always(ctx)
                    //    {
                    //        fz_drop_device(ctx, dev);
                    //        dev = NULL;
                    //        fz_drop_output(ctx, out);
                    //        out = NULL;
                    //        fz_drop_buffer(ctx, buf);
                    //        buf = NULL;
                    //    }
                    //    fz_catch(ctx)
                    //    {
                    //        fz_drop_display_list(ctx, fzlist);
                    //        CString msg;
                    //        msg.Format(_T("cannot dump page %d in file '%s'"), pgnum, pdffilename);
                    //        ReportError(RE_RUNTIME, msg);
                    //        SendDetails(msg);
                    //        m_WasError = true;
                    //        if (fzlist)
                    //            fz_drop_display_list(ctx, fzlist);
                    //        fz_drop_page(ctx, fzpage);
                    //        fzpage = NULL;
                    //        fzlist = NULL;
                    //        DELETEREFOBJECT(kpage);
                    //        break;
                    //    }

                    //}

                    SendDetailsMsg(_T("generate image page %d"), pgnum + 1);

                    //====================================================================================
                    // Save page image
                    //====================================================================================
                    fz_try(ctx)
                    {
                        pagesize = fz_bound_page(ctx, fzpage);

                        float rotation = 0;
                        zoom = (RecalcPageSize(pagesize)* (imgres / 72.0));

                        ctm = fz_pre_scale(fz_rotate(rotation), zoom, zoom);
                        tbounds = pagesize;

                        ibounds = fz_round_rect(fz_transform_rect(tbounds, ctm));
                        //ibounds = fz_round_rect(tbounds);
                        tbounds = fz_rect_from_irect(ibounds);

                        pix = fz_new_pixmap_with_bbox(ctx, colorspace, ibounds,NULL,1);
                        fz_set_pixmap_resolution(ctx, pix, imgres, imgres);
                        fz_clear_pixmap_with_value(ctx, pix, 255);

                        dev = fz_new_draw_device(ctx, fz_identity, pix);
                        fz_enable_device_hints(ctx, dev, FZ_DONT_INTERPOLATE_IMAGES);
                        if (skipdefimage)
                            fz_enable_device_hints(ctx, dev, FZ_CCS_SKIP_INVALID);
                        //						fz_run_page(fzdoc, fzpage, dev, &ctm, &cookie);
                        fz_run_display_list(ctx, fzlist, dev, ctm, tbounds, &cookie);
                        fz_close_device(ctx, dev);
                        fz_drop_device(ctx, dev);
                        dev = NULL;

                        ModChangeSend(_T("image generated"));
                        SendDetailsMsg(_T("move image page %d"), pgnum + 1);

                        //char sz[512];
                        //sprintf(sz, "%S", imgnam);

                        SendDetailsMsg(_T("generated image is x=%d y=%d w=%d h=%d n=%d"), pix->x, pix->y, pix->w, pix->h, pix->n);
                        nextprogressstart++;

                        TIFF * tif=NULL;
                        fz_try(ctx)
                        {
                            if ((tif = TIFFOpen(ToUtf8(tif_output_file), "w")) == NULL) {
                                CString msg;
                                msg.Format(_T("File error on page %d in file '%s'\r\n"), pgnum, CurPDFFile);
                                SendErrorMsg(msg);
                                fz_throw(ctx, 2, ToUtf8(msg));
                            }

                            TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, pix->w);
                            TIFFSetField(tif, TIFFTAG_IMAGELENGTH, pix->h);
                            TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
                            TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
                            TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
                            TIFFSetField(tif, TIFFTAG_XRESOLUTION, (double)(imgres));
                            TIFFSetField(tif, TIFFTAG_YRESOLUTION, (double)(imgres));
                            TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);

                            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
                            TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
                            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
                            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
                        }
                        fz_catch(ctx)
                        {
                            CString msg, msgdata;
                            CString TestPath;
                            MEMORYSTATUSEX memstate;
                            unsigned __int64 i64FreeBytesToCaller, i64TotalTypes, i64FreeBytes;
                            msg.Format(_T("Memory error on page %d in file '%ws'"), pgnum, CurPDFFile);
                            memset(&memstate, 0, sizeof(memstate));
                            memstate.dwLength = sizeof(memstate);
                            GetTempPath(_MAX_PATH, TestPath.GetBuffer(_MAX_PATH + 10));
                            TestPath.ReleaseBuffer();
                            GetDiskFreeSpaceEx(TestPath, (PULARGE_INTEGER)&i64FreeBytesToCaller, (PULARGE_INTEGER)&i64TotalTypes, (PULARGE_INTEGER)&i64FreeBytes);
                            GlobalMemoryStatusEx(&memstate);
                            msgdata.Format(_T("\r\nmemory status is %I32x %d%% (%I64d/%I64d/%I64d/%I64d/%I64d/%I64d)\r\nTemp is %s : (%I64d/%I64d/%I64d) : %d"),
                                           ::GetVersion(), memstate.dwMemoryLoad, memstate.ullTotalPhys / DIV1, memstate.ullAvailPhys / DIV1, memstate.ullTotalPageFile / DIV1, memstate.ullAvailPageFile / DIV1, memstate.ullTotalVirtual / DIV1, memstate.ullAvailVirtual / DIV1,
                                           TestPath, i64FreeBytesToCaller / DIV2, i64TotalTypes / DIV2, i64FreeBytes / DIV2, _taccess(TestPath, 06));
                            SendErrorMsg(msg+msgdata);
                            SendDetails(msg + msgdata);
                            m_WasError = true;
                            fz_rethrow(ctx);
                        }


                        fz_try(ctx)
                        {
                            unsigned char *p = pix->samples;
                            unsigned char *ln = new unsigned char[pix->w*3];

                            for (int i = 0; i < pix->h; i++) {
                                int xl = 0;
                                ScriptAliveCallBack();
                                for (int b = 0; b < pix->w; ++b) {
                                    ln[xl++] = p[0];
                                    ln[xl++] = p[1];
                                    ln[xl++] = p[2];
                                    p += 4;
                                }
                                TIFFWriteScanline(tif, ln, i, 0);
                            }
                            p = NULL;
                            delete[] ln;
                        }
                        fz_catch(ctx)
                        {
                            CString msg;
                            msg.Format(_T("pixel format error on page %d in file '%ws'"), pgnum, CurPDFFile);
                            SendErrorMsg( msg);
                            SendDetails(msg);
                            m_WasError = true;
                            fz_rethrow(ctx);
                        }
                        fz_try(ctx)
                        {
                            TIFFClose(tif);
                        }
                        fz_catch(ctx)
                        {
                            CString msg, msgdata;
                            CString TestPath;
                            MEMORYSTATUSEX memstate;
                            unsigned __int64 i64FreeBytesToCaller, i64TotalTypes, i64FreeBytes;
                            msg.Format(_T("image generation error on page %d in file '%ws'"), pgnum, CurPDFFile);
                            memset(&memstate, 0, sizeof(memstate));
                            memstate.dwLength = sizeof(memstate);
                            GetTempPath(_MAX_PATH, TestPath.GetBuffer(_MAX_PATH + 10));
                            TestPath.ReleaseBuffer();
                            GetDiskFreeSpaceEx(TestPath, (PULARGE_INTEGER)&i64FreeBytesToCaller, (PULARGE_INTEGER)&i64TotalTypes, (PULARGE_INTEGER)&i64FreeBytes);
                            GlobalMemoryStatusEx(&memstate);
                            msgdata.Format(_T("\r\nmemory status is %I32x %d%% (%I64d/%I64d/%I64d/%I64d/%I64d/%I64d)\r\nTemp is %s : (%I64d/%I64d/%I64d) : %d"),
                                           ::GetVersion(), memstate.dwMemoryLoad, memstate.ullTotalPhys / DIV1, memstate.ullAvailPhys / DIV1, memstate.ullTotalPageFile / DIV1, memstate.ullAvailPageFile / DIV1, memstate.ullTotalVirtual / DIV1, memstate.ullAvailVirtual / DIV1,
                                           TestPath, i64FreeBytesToCaller / DIV2, i64TotalTypes / DIV2, i64FreeBytes / DIV2, _taccess(TestPath, 06));
                            SendErrorMsg(msg+msgdata);
                            SendDetails(msg + msgdata);
                            m_WasError = true;
                            fz_rethrow(ctx);
                        }
                        ModChangeSend(_T("image moved"));
                        SendDetailsMsg(_T("save image page %d to %ws"), pgnum + 1, tif_output_file);
                        SendImageName(PIT_TYPE_ORIGINAL, pgnum, tif_output_file);
                        //DELETEOBJECT(img);
                        fz_clear_pixmap(ctx, pix);
                        fz_drop_pixmap(ctx, pix);
                        pix = NULL;
                        ExtFiles.Add(tif_output_file);
                        nextprogressstart++;
                    }
                    fz_catch(ctx)
                    {
                        fz_drop_page(ctx, fzpage);
                        fzpage = NULL;
                        fz_rethrow(ctx);
                    }
                   //====================================================================================
                    //====================================================================================
                    //PIT_TYPE_IMAGEONLY, PIT_TYPE_PATHONLY, PIT_TYPE_IMAGEANDPATH, PIT_TYPE_TEXTONLY
                    //=================================================================================
                    if (!isNwReimport) {
                        RenderPage(PIT_TYPE_IMAGEONLY, 250, m_WasError, ctx, pgnum, imgres, skipdefimage, fzpage, fzlist, cookie, colorspace);
                        nextprogressstart++;
                        RenderPage(PIT_TYPE_PATHONLY, 235, m_WasError, ctx, pgnum, imgres, skipdefimage, fzpage, fzlist, cookie, colorspace);
                        nextprogressstart++;
                    }
                    //=================================================================================



                    //====================================================================================
                    // Save page text
                    //====================================================================================
                    SendDetailsMsg(_T("start text page %d"), pgnum + 1);

                    fz_buffer *buf = NULL,*buf2=NULL;
                    buf = fz_new_buffer(ctx, 1024); //fz_buffer grows automatically (50% more space when needed)
                    buf2 = fz_new_buffer(ctx, 1024); //fz_buffer grows automatically (50% more space when needed)
                    fz_output *out = fz_new_output_with_buffer(ctx, buf);
                    fz_output *out2 = fz_new_output_with_buffer(ctx, buf2);

                    fz_write_string(ctx, out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
                    fz_write_string(ctx, out2, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

                    fz_stext_page *text = NULL;
                    fz_var(text);

                    fz_try(ctx)
                    {
                        fz_matrix ctmt;
                        fz_stext_options opts; opts.flags = 0;

                        opts.flags |= FZ_STEXT_PRESERVE_IMAGES; // | FZ_STEXT_PRESERVE_LIGATURES;
                        if (skipdefimage)
                            opts.flags |= FZ_CCS_SKIP_INVALID;
                        //fz_disable_device_hints(ctx, dev, FZ_IGNORE_IMAGE);

                        float pgzoom = RecalcPageSize(pagesize);

                        fz_pre_scale((ctmt=fz_identity), pgzoom, pgzoom);

                        text = fz_new_stext_page(ctx,bounds);
                        dev = fz_new_stext_device(ctx, text,&opts);

                        fz_run_display_list(ctx, fzlist, dev, ctmt, fz_infinite_rect, &cookie);
                        fz_close_device(ctx, dev);
                        fz_drop_device(ctx, dev);
                        dev = NULL;

                        fz_print_stext_page_as_xmltext(ctx, out, text, pgnum, MainLanguage, CStringA(tif_output_file), AddRectToDoc, NULL, CStringA(FVERSION));
                        ModChangeSend(_T("xml to text"));
                        SendDetailsMsg(_T("parsed text page %d"), pgnum + 1);

                        CString text_output_file = CurOutPath + GetPageFileName(pgnum) + _T("_PGTEXT.xml");
                        if (withpdftext) {
                            if (!WriteStringToFile(text_output_file, fz_string_from_buffer(ctx, buf)))
                                SendErrorMsg(_T("Error on writeing XML text"));
                            else {
                                SendImageName(8, pgnum, text_output_file);
                                ExtFiles.Add(text_output_file);
                            }
                        }
                        if (!isNwReimport) {
                            fz_print_stext_page_as_xmlraw(ctx, out2, text, pgnum, 72, CStringA(FVERSION));
                            ModChangeSend(_T("xml to raw"));
                            SendDetailsMsg(_T("parsed raw text page %d"), pgnum + 1);
                            text_output_file = CurOutPath + GetPageFileName(pgnum) + _T("_RAW.xml");
                            if (!WriteStringToFile(text_output_file, fz_string_from_buffer(ctx, buf2)))
                                SendErrorMsg(_T("Error on writeing RAW XML text"));
                            else {
                                SendImageName(9, pgnum, text_output_file);
                                ExtFiles.Add(text_output_file);
                            }

                        }

                        state++;
                        ModChangeSend(_T("raw text done"));
                    }
                    fz_always(ctx)
                    {
                        fz_drop_output(ctx, out);
                        out = NULL;
                        fz_drop_output(ctx, out2);
                        out2 = NULL;
                        fz_drop_buffer(ctx, buf);
                        buf = NULL;
                        fz_drop_buffer(ctx, buf2);
                        buf2 = NULL;
                        fz_drop_stext_page(ctx, text);
                        text = NULL;
                    }
                    fz_catch(ctx)
                    {

                    }

                    state++;
                    ModChangeSend(_T("text done"));
                    SubProgressSend(0, 100);

                    //====================================================================================
                    //====================================================================================
                    fz_drop_display_list(ctx, fzlist);
                    fzlist = NULL;
                    fz_drop_page(ctx, fzpage);
                    fzpage = NULL;

                    state++;
                    ModChangeSend(_T("page done"));
                    SendDetailsMsg(_T("done page %d"), pgnum + 1);
                }
                catch (...) {
                    CString msg;
                    msg.Format(_T("Import PDF error on state %d"), state);
                    SendErrorMsg( msg);
                    SendDetails(msg);
                    m_WasError = true;
                    break;
                };
#ifdef MAKEDUMP
            }
            _CrtMemCheckpoint(&s2);
            if (_CrtMemDifference(&s3, &s1, &s2)) {
                _CrtMemDumpStatistics(&s3);
                _CrtMemDumpAllObjectsSince(&s1);
            }
#endif
#ifdef TRACEMEM
            if ((pgnum & 3) == 3) {

                fz_document *nfzdoc = fz_open_document(ctx, filename);
                if (nfzdoc) {
                    fz_drop_document(ctx, fzdoc);
                    fzdoc = nfzdoc;
                    dumpptr(startid);
                    startid = memID;
                }
            }
#endif
            SendPageEnd(pgnum, allpganz);
        };
    }else {
        SendErrorMsg(_T("Import PDF with no pages"));
        SendDetails(_T("Import PDF with no pages"));
        m_WasError=true;
    }
    SendDetails(_T("cleanup PDF doc"));
    ModChangeSend(_T("pdf done"));
    fz_drop_document(ctx, fzdoc);
    ModChangeSend(_T("pdf closed"));

    fz_drop_colorspace(ctx, colorspace);

    fz_drop_context(ctx);
    ModChangeSend(_T("context free"));
    return (m_WasError?(isAbort?11:10):0);
}

//===========================================================================
//int __declspec( dllexport ) PdfWork(int argc, TCHAR* argv[])

int PdfWork(int argc, TCHAR* argv[])
{
    CString rdHelp,ExcludeString;
#ifdef addlog
    CStdioFile lgfile(_T("C:\\temp\\PDFLD.log"),CFile::modeCreate|CFile::modeNoTruncate|CFile::modeWrite|CFile::typeText);
#endif
    int nRetCode = 0;

#ifdef addlog
    lgfile.WriteString(_T("    init programm\r\n"));
#endif

    //GetMessageRedirect()=MessageRedirector;
    //GetErrorRedirect()=ErrorRedirector;

    TIFFErrorHandler perr = TIFFSetErrorHandler(PutWin32Error);

#ifdef addlog
    _tprintf(_T("\r\n    init programm\r\n"));
    lgfile.WriteString(_T("    after redirect\r\n"));
#endif
    CoInitialize(NULL);

    //if (OldAlive == NULL) {
    //    OldAlive = SetNewAlive(ScriptAliveCallBack);
    //};

#ifdef addlog
    lgfile.WriteString(_T("    after wininit\r\n"));
#endif
    if (nRetCode == 0) {
        if (argc < 2) {
            _tprintf(_T("\r\n!!! Usage Error: Invalid parameter count\r\n"));
            nRetCode = 2002;
        } else {
            CurPDFFile = argv[1];
            CurOutPath = argv[2];
            if (argc > 2) {
                for (int argnum = 3; argnum < argc; argnum++) {
                    CString tempopt = argv[argnum];
                    if (tempopt.Left(9).CompareNoCase(_T("-exclude:")) == 0) {
                        ExcludeString = _T(",") + tempopt.Mid(9) + _T(",");
                    } else if (tempopt.Left(6).CompareNoCase(_T("-size:")) == 0) {
                        AllPDFMinWidth = _ttoi(tempopt.Mid(6));
                    } else if (tempopt.Left(5).CompareNoCase(_T("-res:")) == 0) {
                        DefaultRes = _ttoi(tempopt.Mid(5));
                        if (DefaultRes < 72) DefaultRes = 300;
                    } else if (tempopt.Left(10).CompareNoCase(_T("-language:")) == 0) {
                        MainLanguage=_ttoi(tempopt.Mid(10));                        
                    } else if (tempopt.CompareNoCase(_T("-verbose")) == 0) {
                        WithProgressMsg=true;
                    } else if (tempopt.CompareNoCase(_T("-debug")) == 0) {
                        InitDatailLog();
                    } else if (tempopt.CompareNoCase(_T("-skipinvalid")) == 0) {
                        skipdefimage = true;
                    } else if (tempopt.CompareNoCase(_T("-notext")) == 0) {
                        withpdftext = false;
                    } else {
                        //if (tempopt.CompareNoCase(_T("-nomsg")))
                        //  withtext = false;
                        //else {
                        _tprintf(_T("\r\n!!! Usage Error: Invalid parameter %s  \r\n"), (LPCWSTR)tempopt);
                        nRetCode = 2002;
                        break;
                        //}
                    }
                }
            }
        }
    }
#ifdef addlog
    lgfile.WriteString(_T("    after paramparse\r\n"));
#endif

        if (nRetCode == 0) {
            AbortFileName = CurOutPath + _T(".ABORT");
            nRetCode = LoadPdfFile(ExcludeString);
        }

        if (nRetCode == 0) {
            // pDoc->Save();
            if ((_taccess(AbortFileName, 0) == 0)) {
                nRetCode = 2201;
            } else {
                SendDetailsMsg(_T("save doc"));
            }
        }

        if (nRetCode != 0) {
            //cleanup files
            SendDetailsMsg(_T("Start cleanup"));
            try {
                for (int df = 0; df < ExtFiles.GetCount(); df++)
                    DeleteFile(ExtFiles[df]);
            }
            catch (...) { ASSERT(FALSE); };
        }
 

    try
    {
        SendDetailsMsg(_T("Start exitproc"));
        if (AfxGetApp())
            if (AfxGetApp()->m_lpfnDaoTerm)
                AfxGetApp()->m_lpfnDaoTerm();
        CoUninitialize();
        SendDetailsMsg(_T("  End programm"));
        _tprintf(_T("\r\n    End programm\r\n"));
    }
    catch(...)
    {
        SendDetailsMsg(_T("!!! Fatal Error: Error while exiting PDF Loader on PDF filer %s"),(LPCWSTR)CurPDFFile);
        _tprintf(_T("\r\n!!! Fatal Error: Error while exiting PDF Loader on PDF filer %s\r\n"),(LPCWSTR)CurPDFFile);
        if (nRetCode==0)
            nRetCode=2101;
    }

    TIFFSetErrorHandler(perr);

    return nRetCode;
}
//===========================================================================
