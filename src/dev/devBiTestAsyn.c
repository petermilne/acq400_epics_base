/* devBiTestAsyn.c */
/* share/src/dev $Id$ */

/* devBiTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02  01-08-92        jba     Added cast in call to wdStart to avoid compile warning msg
 * .03  02-05-92	jba	Changed function arguments from paddr to precord 
 *      ...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<wdLib.h>

#include	<alarm.h>
#include	<callback.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<biRecord.h>

/* Create the dset for devBiTestAsyn */
long init_record();
long read_bi();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;
	DEVSUPFUN	special_linconv;
}devBiTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_bi,
	NULL};

/* control block for callback*/
struct callback {
	void (*callback)();
	int priority;
	struct dbCommon *prec;
	WDOG_ID wd_id;
	void (*process)();
};

void callbackRequest();

static void myCallback(pcallback)
    struct callback *pcallback;
{
    struct biRecord *pbi=(struct biRecord *)(pcallback->prec);
    struct rset     *prset=(struct rset *)(pbi->rset);

    dbScanLock((struct dbCommon *)pbi);
    (*prset->process)(pbi);
    dbScanUnlock((struct dbCommon *)pbi);
}
    
    

static long init_record(pbi)
    struct biRecord	*pbi;
{
    char message[100];
    struct callback *pcallback;

    /* bi.inp must be a CONSTANT*/
    switch (pbi->inp.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pbi->dpvt = (caddr_t)pcallback;
	pcallback->callback = myCallback;
	pcallback->priority = priorityLow;
	pcallback->prec = (struct dbCommon *)pbi;
	pcallback->wd_id = wdCreate();
	pbi->val = pbi->inp.value.value;
	pbi->udf = FALSE;
	break;
    default :
	strcpy(message,pbi->name);
	strcat(message,": devBiTestAsyn (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_bi(pbi)
    struct biRecord	*pbi;
{
    char message[100];
    long status,options,nRequest;
    struct callback *pcallback=(struct callback *)(pbi->dpvt);
    int		wait_time;

    /* bi.inp must be a CONSTANT*/
    switch (pbi->inp.type) {
    case (CONSTANT) :
	if(pbi->pact) {
		printf("%s Completed\n",pbi->name);
		return(2); /* don't convert */
	} else {
		wait_time = (int)(pbi->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		printf("%s Starting asynchronous processing\n",pbi->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,(int)pcallback);
		return(1);
	}
    default :
        if(recGblSetSevr(pbi,SOFT_ALARM,VALID_ALARM)){
		if(pbi->stat!=SOFT_ALARM) {
			strcpy(message,pbi->name);
			strcat(message,": devBiTestAsyn (read_bi) Illegal INP field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
