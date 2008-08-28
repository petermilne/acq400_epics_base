/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* $Id$
 * 
 * Record Support Routines for the Array Subroutine Record type,
 * derived from Andy Foster's genSub record, with some features
 * removed and asynchronous support added.
 *
 *      Original Author: Andy Foster
 *      Revised by: Andrew Johnson
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cantProceed.h"
#include "dbDefs.h"
#include "dbEvent.h"
#include "dbAccess.h"
#include "dbFldTypes.h"
#include "dbStaticLib.h"
#include "errMdef.h"
#include "recSup.h"
#include "devSup.h"
#include "special.h"
#include "registryFunction.h"
#include "epicsExport.h"
#include "recGbl.h"

#define GEN_SIZE_OFFSET
#include "aSubRecord.h"
#undef  GEN_SIZE_OFFSET


typedef long (*GENFUNCPTR)(struct aSubRecord *);

/* Create RSET - Record Support Entry Table*/

#define report             NULL
#define initialize          NULL
static long init_record(aSubRecord *, int);
static long process(aSubRecord *);
static long special(DBADDR *, int);
static long get_value(aSubRecord *, struct valueDes *);
static long cvt_dbaddr(DBADDR *);
static long get_array_info(DBADDR *, long *, long *);
static long put_array_info(DBADDR *, long );
#define get_units          NULL
static long get_precision(DBADDR *, long *);
#define get_enum_str       NULL
#define get_enum_strs      NULL
#define put_enum_str       NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double   NULL

rset aSubRSET = {
    RSETNUMBER,
    report,
    initialize,
    init_record,
    process,
    special,
    get_value,
    cvt_dbaddr,
    get_array_info,
    put_array_info,
    get_units,
    get_precision,
    get_enum_str,
    get_enum_strs,
    put_enum_str,
    get_graphic_double,
    get_control_double,
    get_alarm_double
};
epicsExportAddress(rset, aSubRSET);

static long initFields(epicsEnum16 *pft, epicsUInt32 *pno, epicsUInt32 *pne,
    const char **fldnames, void **pval, void **povl);
static long fetch_values(aSubRecord *prec);
static void monitor(aSubRecord *);
static long do_sub(aSubRecord *);

#define NUM_ARGS        21
#define MAX_ARRAY_SIZE 10000000

/* These are the names of the Input fields */
static const char *Ifldnames[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
    "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U"
};

/* These are the names of the Output fields */
static const char *Ofldnames[] = {
    "VALA", "VALB", "VALC", "VALD", "VALE", "VALF", "VALG",
    "VALH", "VALI", "VALJ", "VALK", "VALL", "VALM", "VALN",
    "VALO", "VALP", "VALQ", "VALR", "VALS", "VALT", "VALU"
};


static long init_record(aSubRecord *prec, int pass)
{
    GENFUNCPTR     pfunc;
    long           status;
    int            i;

    status = 0;
    if (pass == 0) {
        /* Allocate memory for arrays */
        initFields(&prec->fta,  &prec->noa,  &prec->nea,  Ifldnames,
            &prec->a,    NULL);
        initFields(&prec->ftva, &prec->nova, &prec->neva, Ofldnames,
            &prec->vala, &prec->ovla);
        return 0;
    }

    /* Initialize the Subroutine Name Link */
    switch (prec->subl.type) {
    case CONSTANT:
        recGblInitConstantLink(&prec->subl, DBF_STRING, prec->snam);
        break;

    case PV_LINK:
    case DB_LINK:
    case CA_LINK:
        break;

    default:
        recGblRecordError(S_db_badField, (void *)prec,
            "aSubRecord(init_record) Bad SUBL link type");
        return S_db_badField;
    }

    /* Initialize Input Links */
    for (i = 0; i < NUM_ARGS; i++) {
        struct link *plink = &(&prec->inpa)[i];
        switch (plink->type) {
        case CONSTANT:
            if ((&prec->noa)[i] < 2) {
                if (recGblInitConstantLink(plink, (&prec->fta)[i], (&prec->a)[i])) {
                    prec->udf = FALSE;
                } else
                    prec->udf = TRUE;
            }
            break;

        case PV_LINK:
        case CA_LINK:
        case DB_LINK:
            break;

        default:
            recGblRecordError(S_db_badField, (void *)prec,
                "aSubRecord(init_record) Illegal INPUT LINK");
            status = S_db_badField;
            break;
        }
    }

    if (status)
        return status;

    /* Initialize Output Links */
    for (i = 0; i < NUM_ARGS; i++) {
        switch ((&prec->outa)[i].type) {
        case CONSTANT:
        case PV_LINK:
        case CA_LINK:
        case DB_LINK:
            break;

        default:
            recGblRecordError(S_db_badField, (void *)prec,
                "aSubRecord(init_record) Illegal OUTPUT LINK");
            status = S_db_badField;
        }
    }
    if (status)
        return status;

    /* Call the user initialization routine if there is one */
    if (prec->inam[0] != 0) {
        pfunc = (GENFUNCPTR)registryFunctionFind(prec->inam);
        if (pfunc) {
            pfunc(prec);
        } else {
            recGblRecordError(S_db_BadSub, (void *)prec,
                "aSubRecord::init_record - INAM subr not found");
            return S_db_BadSub;
        }
    }

    if (prec->lflg == aSubLFLG_IGNORE &&
        prec->snam[0] != 0) {
        pfunc = (GENFUNCPTR)registryFunctionFind(prec->snam);
        if (pfunc)
            prec->sadr = pfunc;
        else {
            recGblRecordError(S_db_BadSub, (void *)prec,
                "aSubRecord::init_record - SNAM subr not found");
            return S_db_BadSub;
        }
    }
    return 0;
}


static long initFields(epicsEnum16 *pft, epicsUInt32 *pno, epicsUInt32 *pne,
    const char **fldnames, void **pval, void **povl)
{
    int i;
    long status = 0;

    for (i = 0; i < NUM_ARGS; i++, pft++, pno++, pne++, pval++) {
        epicsUInt32 num;
        epicsUInt32 flen;

        if (*pft > DBF_ENUM)
            *pft = DBF_CHAR;

        if (*pno == 0)
            *pno = 1;

        flen = dbValueSize(*pft);
        num = *pno * flen;

        if (num > MAX_ARRAY_SIZE) {
            epicsPrintf("Link %s - Array too large! %d Bytes\n", fldnames[i], num);
            *pno = num = 0;
            status = S_db_errArg;
        } else
            *pval = (char *)callocMustSucceed(*pno, flen,
                "aSubRecord::init_record");

        *pne = *pno;

        if (povl) {
            if (num)
                *povl = (char *)callocMustSucceed(*pno, flen,
                    "aSubRecord::init_record");
            povl++;
        }
    }
    return status;
}


static long process(aSubRecord *prec)
{
    int pact = prec->pact;
    long status = 0;

    if (!pact) {
        prec->pact = TRUE;
        status = fetch_values(prec);
        prec->pact = FALSE;
    }

    if (!status) {
        status = do_sub(prec);
        prec->val = status;
    }

    if (!pact && prec->pact)
        return 0;

    prec->pact = TRUE;

    /* Push the output link values */
    if (!status) {
        int i;
        for (i = 0; i < NUM_ARGS; i++)
            dbPutLink(&(&prec->outa)[i], (&prec->ftva)[i], (&prec->vala)[i],
                (&prec->neva)[i]);
    }

    recGblGetTimeStamp(prec);
    monitor(prec);
    recGblFwdLink(prec);
    prec->pact = FALSE;

    return 0;
}

static long fetch_values(aSubRecord *prec)
{
    long status;
    int i;

    if (prec->lflg == aSubLFLG_READ) {
        /* Get the Subroutine Name and look it up if changed */
        status = dbGetLink(&prec->subl, DBR_STRING, prec->snam, 0, 0);
        if (status)
            return status;

        if (prec->snam[0] != 0 &&
            strcmp(prec->snam, prec->onam)) {
            GENFUNCPTR pfunc = (GENFUNCPTR)registryFunctionFind(prec->snam);

            if (!pfunc)
                return S_db_BadSub;

            prec->sadr = pfunc;
            strcpy(prec->onam, prec->snam);
        }
    }

    /* Get the input link values */
    for (i = 0; i < NUM_ARGS; i++) {
        long nRequest = (&prec->noa)[i];
        status = dbGetLink(&(&prec->inpa)[i], (&prec->fta)[i], (&prec->a)[i], 0,
            &nRequest);
        if (nRequest > 0)
            (&prec->nea)[i] = nRequest;
        if (status)
            return status;
    }
    return 0;
}

static long get_precision(DBADDR *paddr, long *precision)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;

    *precision = prec->prec;
    recGblGetPrec(paddr, precision);
    return 0;
}


static long get_value(aSubRecord *prec, struct valueDes *pvdes)
{
    pvdes->no_elements = 1;
    pvdes->pvalue      = (void *)(&prec->val);
    pvdes->field_type  = DBF_LONG;
    return 0;
}


static void monitor(aSubRecord *prec)
{
    int            i;
    unsigned short monitor_mask;

    monitor_mask = recGblResetAlarms(prec) | DBE_VALUE | DBE_LOG;

    /* Post events for VAL field */
    if (prec->val != prec->oval) {
        db_post_events(prec, &prec->val, monitor_mask);
        prec->oval = prec->val;
    }

    /* Event posting on VAL arrays depends on the setting of prec->eflg */
    switch (prec->eflg) {
    case aSubEFLG_NEVER:
        break;
    case aSubEFLG_ON_CHANGE:
        for (i = 0; i < NUM_ARGS; i++) {
            epicsUInt32 alen = dbValueSize((&prec->ftva)[i]) * (&prec->neva)[i];
            void *povl = (&prec->ovla)[i];
            void *pval = (&prec->vala)[i];
            if (memcmp(povl, pval, alen)) {
                memcpy(povl, pval, alen);
                db_post_events(prec, pval, monitor_mask);
            }
        }
        break;
    case aSubEFLG_ALWAYS:
        for (i = 0; i < NUM_ARGS; i++)
            db_post_events(prec, (&prec->vala)[i], monitor_mask);
        break;
    }
    return;
}


static long do_sub(aSubRecord *prec)
{
    GENFUNCPTR pfunc = prec->sadr;
    long status;

    if (prec->snam[0] == 0)
        return 0;

    if (pfunc == NULL) {
        recGblSetSevr(prec, BAD_SUB_ALARM, INVALID_ALARM);
        return S_db_BadSub;
    }
    status = pfunc(prec);
    if (status < 0)
        recGblSetSevr(prec, SOFT_ALARM, prec->brsv);
    else
        prec->udf = FALSE;

    return status;
}


static long cvt_dbaddr(DBADDR *paddr)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex >= aSubRecordA &&
        fieldIndex <= aSubRecordU) {
        int offset = fieldIndex - aSubRecordA;
        paddr->pfield      = (&prec->a  )[offset];
        paddr->no_elements = (&prec->noa)[offset];
        paddr->field_type  = (&prec->fta)[offset];
    }
    else if (fieldIndex >= aSubRecordVALA &&
             fieldIndex <= aSubRecordVALU) {
        int offset = fieldIndex - aSubRecordVALA;
        paddr->pfield      = (&prec->vala)[offset];
        paddr->no_elements = (&prec->nova)[offset];
        paddr->field_type  = (&prec->ftva)[offset];
    }
    else {
        errlogPrintf("aSubRecord::cvt_dbaddr called for %s.%s\n",
            prec->name, ((dbFldDes *)paddr->pfldDes)->name);
        return 0;
    }
    paddr->dbr_field_type = paddr->field_type;
    paddr->field_size     = dbValueSize(paddr->field_type);
    return 0;
}


static long get_array_info(DBADDR *paddr, long *no_elements, long *offset)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex >= aSubRecordA &&
        fieldIndex <= aSubRecordU) {
        *no_elements = (&prec->nea)[fieldIndex - aSubRecordA];
    }
    else if (fieldIndex >= aSubRecordVALA &&
             fieldIndex <= aSubRecordVALU) {
        *no_elements = (&prec->neva)[fieldIndex - aSubRecordVALA];
    }
    else {
        errlogPrintf("aSubRecord::get_array_info called for %s.%s\n",
            prec->name, ((dbFldDes *)paddr->pfldDes)->name);
    }
    *offset = 0;

    return 0;
}


static long put_array_info(DBADDR *paddr, long nNew)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex >= aSubRecordA &&
        fieldIndex <= aSubRecordU) {
        (&prec->nea)[fieldIndex - aSubRecordA] = nNew;
    }
    else if (fieldIndex >= aSubRecordVALA &&
             fieldIndex <= aSubRecordVALU) {
        (&prec->neva)[fieldIndex - aSubRecordVALA] = nNew;
    }
    else {
        errlogPrintf("aSubRecord::put_array_info called for %s.%s\n",
            prec->name, ((dbFldDes *)paddr->pfldDes)->name);
    }
    return 0;
}


static long special(DBADDR *paddr, int after)
{
    aSubRecord *prec = (aSubRecord *)paddr->precord;

    if (after &&
        prec->lflg == aSubLFLG_IGNORE) {
        if (prec->snam[0] == 0)
            return 0;

        prec->sadr = (GENFUNCPTR)registryFunctionFind(prec->snam);
        if (!prec->sadr) {
            recGblRecordError(S_db_BadSub, (void *)prec, prec->snam);
            return S_db_BadSub;
        }
    }
    return 0;
}