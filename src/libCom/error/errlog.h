/* src/libCom/errlog.h */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#ifndef INCerrlogh
#define INCerrlogh

#include <stdarg.h>

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif


/* define errMessage with a macro so we can print the file and line number*/
#define errMessage(S, PM) \
         errPrintf(S, __FILE__, __LINE__, PM)
/* epicsPrintf and epicsVprintf old versions of errlog routines*/
#define epicsPrintf errlogPrintf
#define epicsVprintf errlogVprintf

typedef void(*errlogListener) (void *pPrivate, const char *message);

typedef enum {errlogInfo,errlogMinor,errlogMajor,errlogFatal} errlogSevEnum;

#ifdef ERRLOG_INIT
epicsShareDef char * errlogSevEnumString[] = {"info","minor","major","fatal"};
#else
epicsShareExtern char * errlogSevEnumString[];
#endif

epicsShareFunc int epicsShareAPIV errlogPrintf(
    const char *pformat, ...);
epicsShareFunc int epicsShareAPIV errlogVprintf(
    const char *pformat,va_list pvar);
epicsShareFunc int epicsShareAPIV errlogSevPrintf(
    const errlogSevEnum severity,const char *pformat, ...);
epicsShareFunc int epicsShareAPIV errlogSevVprintf(
    const errlogSevEnum severity,const char *pformat,va_list pvar);
epicsShareFunc int epicsShareAPI errlogMessage(
	const char *message);

epicsShareFunc char * epicsShareAPI errlogGetSevEnumString(
    const errlogSevEnum severity);
epicsShareFunc void epicsShareAPI errlogSetSevToLog(
    const errlogSevEnum severity );
epicsShareFunc errlogSevEnum epicsShareAPI errlogGetSevToLog(void);

epicsShareFunc void epicsShareAPI errlogAddListener(
    errlogListener listener, void *pPrivate);
epicsShareFunc void epicsShareAPI errlogRemoveListener(
    errlogListener listener);

epicsShareFunc int epicsShareAPI eltc(int yesno);
epicsShareFunc int epicsShareAPI errlogInit(int bufsize);
epicsShareFunc void epicsShareAPI errlogStop(void);

/*other routines that write to log file*/
epicsShareFunc void epicsShareAPIV errPrintf(long status, const char *pFileName,
    int lineno, const char *pformat, ...);

epicsShareExtern int errVerbose;

#ifdef __cplusplus
}
#endif

#endif /*INCerrlogh*/
