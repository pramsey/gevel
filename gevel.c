#include "postgres.h"

#include "access/genam.h"
#include "access/gist.h"
#include "access/gist_private.h"
#include "access/gistscan.h"
#include "access/heapam.h"
#include "catalog/index.h"
#include "miscadmin.h"
#include "storage/lmgr.h"
#include "catalog/namespace.h"
#include "utils/builtins.h"
#include <fmgr.h>
#include <funcapi.h>
#include <access/heapam.h>
#include <catalog/pg_type.h>

#define PAGESIZE 	(BLCKSZ - MAXALIGN(sizeof(PageHeaderData) + sizeof(ItemIdData)))

#ifndef PG_NARGS
#define PG_NARGS() (fcinfo->nargs)
#endif

static char
*t2c(text* in) {
        char *out=palloc( VARSIZE(in) );
        memcpy(out, VARDATA(in), VARSIZE(in)-VARHDRSZ);
        out[ VARSIZE(in)-VARHDRSZ ] ='\0';
        return out;
}

typedef struct {
	int maxlevel;
	text	*txt;
	char	*ptr;
	int 	len;	
} IdxInfo;


#ifdef PG_MODULE_MAGIC
/* >= 8.2 */ 

PG_MODULE_MAGIC;

static Relation
gist_index_open(RangeVar *relvar) {
	Oid relOid = RangeVarGetRelid(relvar, false);
	return index_open(relOid, AccessExclusiveLock);
}

#define	gist_index_close(r)	index_close((r), AccessExclusiveLock)

#else /* <8.2 */

static Relation
gist_index_open(RangeVar *relvar) {
	Relation rel = index_openrv(relvar);

	LockRelation(rel, AccessExclusiveLock);
	return rel;
}

static void
gist_index_close(Relation rel) {
	UnlockRelation(rel, AccessExclusiveLock);
	index_close(rel);
}

#endif

#if PG_VERSION_NUM >= 80300
#define stringToQualifiedNameList(x,y)	stringToQualifiedNameList(x)
#endif

#if PG_VERSION_NUM < 80300
#define SET_VARSIZE(p,l)	VARATT_SIZEP(p)=(l)
#endif

static void
gist_dumptree(Relation r, int level, BlockNumber blk, OffsetNumber coff, IdxInfo *info) {
	Buffer		buffer;
	Page		page;
	IndexTuple	which;
	ItemId		iid;
	OffsetNumber i,
				maxoff;
	BlockNumber cblk;
	char	   *pred;

	pred = (char *) palloc(sizeof(char) * level * 4 + 1);
	MemSet(pred, ' ', level*4);
	pred[level*4] = '\0';

	buffer = ReadBuffer(r, blk);
	page = (Page) BufferGetPage(buffer);

	maxoff = PageGetMaxOffsetNumber(page);


	while ( (info->ptr-((char*)info->txt)) + level*4 + 128 >= info->len ) {
		int dist=info->ptr-((char*)info->txt);
		info->len *= 2;
		info->txt=(text*)repalloc(info->txt, info->len);
		info->ptr = ((char*)info->txt)+dist;
	}

	sprintf(info->ptr, "%s%d(l:%d) blk: %d numTuple: %d free: %db(%.2f%%) rightlink:%u (%s)\n", 
		pred,
		coff, 
		level, 
		(int) blk,
		(int) maxoff, 
		PageGetFreeSpace(page),  
		100.0*(((float)PAGESIZE)-(float)PageGetFreeSpace(page))/((float)PAGESIZE),
		GistPageGetOpaque(page)->rightlink,
		( GistPageGetOpaque(page)->rightlink == InvalidBlockNumber ) ? "InvalidBlockNumber" : "OK" );
	info->ptr=strchr(info->ptr,'\0');

	if (!GistPageIsLeaf(page) && ( info->maxlevel<0 || level<info->maxlevel ) )
		for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i)) {
			iid = PageGetItemId(page, i);
			which = (IndexTuple) PageGetItem(page, iid);
			cblk = ItemPointerGetBlockNumber(&(which->t_tid));
			gist_dumptree(r, level + 1, cblk, i, info);
		}
	ReleaseBuffer(buffer);
	pfree(pred);
}

PG_FUNCTION_INFO_V1(gist_tree);
Datum	gist_tree(PG_FUNCTION_ARGS);
Datum
gist_tree(PG_FUNCTION_ARGS) {
	text	*name=PG_GETARG_TEXT_P(0);
	char *relname=t2c(name);
	RangeVar   *relvar;
	Relation        index;
	List       *relname_list;
	IdxInfo	info;

	relname_list = stringToQualifiedNameList(relname, "gist_tree");
	relvar = makeRangeVarFromNameList(relname_list);
	index = gist_index_open(relvar);
	PG_FREE_IF_COPY(name,0);

	info.maxlevel = ( PG_NARGS() > 1 ) ? PG_GETARG_INT32(1) : -1; 
	info.len=1024;
	info.txt=(text*)palloc( info.len );
	info.ptr=((char*)info.txt)+VARHDRSZ;

	gist_dumptree(index, 0, GIST_ROOT_BLKNO, 0, &info);

	gist_index_close(index);
	pfree(relname);

	SET_VARSIZE(info.txt, info.ptr-((char*)info.txt));
	PG_RETURN_POINTER(info.txt);
}

typedef struct {
	int 	level;
	int	numpages;
	int 	numleafpages;
	int 	numtuple;
	int	numinvalidtuple;
	int 	numleaftuple;
	uint64	tuplesize;
	uint64	leaftuplesize;
	uint64	totalsize;
} IdxStat;

static void
gist_stattree(Relation r, int level, BlockNumber blk, OffsetNumber coff, IdxStat *info) {
	Buffer		buffer;
	Page		page;
	IndexTuple	which;
	ItemId		iid;
	OffsetNumber i,
				maxoff;
	BlockNumber cblk;
	char	   *pred;

	pred = (char *) palloc(sizeof(char) * level * 4 + 1);
	MemSet(pred, ' ', level*4);
	pred[level*4] = '\0';

	buffer = ReadBuffer(r, blk);
	page = (Page) BufferGetPage(buffer);

	maxoff = PageGetMaxOffsetNumber(page);

	info->numpages++;
	info->tuplesize+=PAGESIZE-PageGetFreeSpace(page);
	info->totalsize+=BLCKSZ;
	info->numtuple+=maxoff;
	if ( info->level < level )
		info->level = level;

	if (GistPageIsLeaf(page)) {
		info->numleafpages++;
		info->leaftuplesize+=PAGESIZE-PageGetFreeSpace(page);
		info->numleaftuple+=maxoff;
	} else {
		for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i)) {
			iid = PageGetItemId(page, i);
			which = (IndexTuple) PageGetItem(page, iid);
			if ( GistTupleIsInvalid(which) )
				info->numinvalidtuple++;
			cblk = ItemPointerGetBlockNumber(&(which->t_tid));
				gist_stattree(r, level + 1, cblk, i, info);
		}
	}

	ReleaseBuffer(buffer);
	pfree(pred);
}

PG_FUNCTION_INFO_V1(gist_stat);
Datum	gist_stat(PG_FUNCTION_ARGS);
Datum
gist_stat(PG_FUNCTION_ARGS) {
	text	*name=PG_GETARG_TEXT_P(0);
	char *relname=t2c(name);
	RangeVar   *relvar;
	Relation        index;
	List       *relname_list;
	IdxStat	info;
	text *out=(text*)palloc(1024);
	char *ptr=((char*)out)+VARHDRSZ;


	relname_list = stringToQualifiedNameList(relname, "gist_tree");
	relvar = makeRangeVarFromNameList(relname_list);
	index = gist_index_open(relvar);
	PG_FREE_IF_COPY(name,0);

	memset(&info, 0, sizeof(IdxStat));

	gist_stattree(index, 0, GIST_ROOT_BLKNO, 0, &info);

	gist_index_close(index);
	pfree(relname);

	sprintf(ptr, 
		"Number of levels:          %d\n"
		"Number of pages:           %d\n"
		"Number of leaf pages:      %d\n"
		"Number of tuples:          %d\n"
		"Number of invalid tuples:  %d\n"
		"Number of leaf tuples:     %d\n"
		"Total size of tuples:      "INT64_FORMAT" bytes\n"
		"Total size of leaf tuples: "INT64_FORMAT" bytes\n"
		"Total size of index:       "INT64_FORMAT" bytes\n",
		info.level+1,
		info.numpages,
		info.numleafpages,
		info.numtuple,
		info.numinvalidtuple,
		info.numleaftuple,
		info.tuplesize,
		info.leaftuplesize,
		info.totalsize);

	ptr=strchr(ptr,'\0');
		 
	SET_VARSIZE(out, ptr-((char*)out));
	PG_RETURN_POINTER(out);
}

typedef struct GPItem {
	Buffer	buffer;
	Page	page;
	OffsetNumber	offset;
	int	level;
	struct GPItem *next;
} GPItem;

typedef struct {
	List	*relname_list;
	RangeVar   *relvar;
	Relation        index;
	Datum	*dvalues;
	char	*nulls;
	GPItem	*item;
} TypeStorage;

static GPItem*
openGPPage( FuncCallContext *funcctx, BlockNumber blk ) {
	GPItem	*nitem;
	MemoryContext     oldcontext;
	Relation index = ( (TypeStorage*)(funcctx->user_fctx) )->index;
	
	oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
	nitem = (GPItem*)palloc( sizeof(GPItem) );
	memset(nitem,0,sizeof(GPItem));

	nitem->buffer = ReadBuffer(index, blk);
	nitem->page = (Page) BufferGetPage(nitem->buffer);
	nitem->offset=FirstOffsetNumber;
	nitem->next = ( (TypeStorage*)(funcctx->user_fctx) )->item;
	nitem->level = ( nitem->next ) ? nitem->next->level+1 : 1; 
	( (TypeStorage*)(funcctx->user_fctx) )->item = nitem;

	MemoryContextSwitchTo(oldcontext);
	return nitem;
} 

static GPItem*
closeGPPage( FuncCallContext *funcctx ) {
	GPItem  *oitem = ( (TypeStorage*)(funcctx->user_fctx) )->item;

	( (TypeStorage*)(funcctx->user_fctx) )->item = oitem->next;
	
	ReleaseBuffer(oitem->buffer);
	pfree( oitem );
	return ( (TypeStorage*)(funcctx->user_fctx) )->item; 
}

static void
setup_firstcall(FuncCallContext  *funcctx, text *name) {
	MemoryContext     oldcontext;
	TypeStorage     *st;
	char *relname=t2c(name);
	TupleDesc            tupdesc;
	char            attname[NAMEDATALEN];
	int i;

	oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

	st=(TypeStorage*)palloc( sizeof(TypeStorage) );
	memset(st,0,sizeof(TypeStorage));
	st->relname_list = stringToQualifiedNameList(relname, "gist_tree");
	st->relvar = makeRangeVarFromNameList(st->relname_list);
	st->index = gist_index_open(st->relvar);
	funcctx->user_fctx = (void*)st;

	tupdesc = CreateTemplateTupleDesc(st->index->rd_att->natts+2, false);
	TupleDescInitEntry(tupdesc, 1, "level", INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, 2, "valid", BOOLOID, -1, 0);
	for (i = 0; i < st->index->rd_att->natts; i++) {
		sprintf(attname, "z%d", i+2);
		TupleDescInitEntry(
			tupdesc,
			i+3,
			attname,
			st->index->rd_att->attrs[i]->atttypid,
			st->index->rd_att->attrs[i]->atttypmod,
			st->index->rd_att->attrs[i]->attndims
		);
	}

	st->dvalues = (Datum *) palloc((tupdesc->natts+2) * sizeof(Datum));
	st->nulls = (char *) palloc((tupdesc->natts+2) * sizeof(char));

	funcctx->slot = TupleDescGetSlot(tupdesc);
	funcctx->attinmeta = TupleDescGetAttInMetadata(tupdesc);

	MemoryContextSwitchTo(oldcontext);
	pfree(relname);

	st->item=openGPPage(funcctx, GIST_ROOT_BLKNO);
} 

static void 
close_call( FuncCallContext  *funcctx ) {
	TypeStorage *st = (TypeStorage*)(funcctx->user_fctx);
	
	while( st->item && closeGPPage(funcctx) );
	
	pfree( st->dvalues );
	pfree( st->nulls );

	gist_index_close(st->index);
}

PG_FUNCTION_INFO_V1(gist_print);
Datum	gist_print(PG_FUNCTION_ARGS);
Datum
gist_print(PG_FUNCTION_ARGS) {
	FuncCallContext  *funcctx;
	TypeStorage     *st;
	Datum result=(Datum)0;
	ItemId          iid;
	IndexTuple      ituple;
	HeapTuple       htuple;
	int i;
	bool isnull;

	if (SRF_IS_FIRSTCALL()) {
		text    *name=PG_GETARG_TEXT_P(0);
		funcctx = SRF_FIRSTCALL_INIT();
		setup_firstcall(funcctx, name);
		PG_FREE_IF_COPY(name,0);
	}

	funcctx = SRF_PERCALL_SETUP();	
	st = (TypeStorage*)(funcctx->user_fctx);

	if ( !st->item ) {
		close_call(funcctx);
		SRF_RETURN_DONE(funcctx);
	}

	while( st->item->offset > PageGetMaxOffsetNumber( st->item->page ) ) {
		if ( ! closeGPPage(funcctx) ) {
			close_call(funcctx);
			SRF_RETURN_DONE(funcctx);
		} 
	}

	iid = PageGetItemId( st->item->page, st->item->offset );
	ituple = (IndexTuple) PageGetItem(st->item->page, iid);

	st->dvalues[0] = Int32GetDatum( st->item->level );
	st->nulls[0] = ' ';
	st->dvalues[1] = BoolGetDatum( (!GistPageIsLeaf(st->item->page) && GistTupleIsInvalid(ituple)) ? false : true );
	st->nulls[1] = ' ';
	for(i=2; i<funcctx->attinmeta->tupdesc->natts; i++) {
		if ( !GistPageIsLeaf(st->item->page) && GistTupleIsInvalid(ituple) ) {
			st->dvalues[i] = (Datum)0;
			st->nulls[i] = 'n';
		} else {
			st->dvalues[i] = index_getattr(ituple, i-1, st->index->rd_att, &isnull); 
			st->nulls[i] = ( isnull ) ? 'n' : ' ';
		}
	}

	htuple = heap_formtuple(funcctx->attinmeta->tupdesc, st->dvalues, st->nulls);
	result = TupleGetDatum(funcctx->slot, htuple);
	st->item->offset = OffsetNumberNext(st->item->offset);
	if ( !GistPageIsLeaf(st->item->page) )
		openGPPage(funcctx, ItemPointerGetBlockNumber(&(ituple->t_tid)) );

	SRF_RETURN_NEXT(funcctx, result);
}

