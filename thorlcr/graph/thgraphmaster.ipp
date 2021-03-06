/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#ifndef _THGRAPHMASTER_IPP
#define _THGRAPHMASTER_IPP

#include "jmisc.hpp"
#include "jsuperhash.hpp"
#include "workunit.hpp"
#include "eclhelper.hpp"
#include "dllserver.hpp"

#include "thcodectx.hpp"
#include "thormisc.hpp"
#include "thgraph.hpp"
#include "thgraphmaster.hpp"
#include "thmfilemanager.hpp"
#include "thgraphmanager.hpp"


interface ILoadedDllEntry;
interface IConstWUGraphProgress;

class CJobMaster;
class CMasterGraphElement;
class graphmaster_decl CMasterGraph : public CGraphBase
{
    CJobMaster *jobM;
    CriticalSection createdCrit;
    Owned<IFatalHandler> fatalHandler;
    CriticalSection exceptCrit;
    bool sentGlobalInit = false;

    CReplyCancelHandler activityInitMsgHandler, bcastMsgHandler, executeReplyMsgHandler;

    void sendQuery();
    void jobDone();
    void sendGraph();
    void getFinalProgress();
    void configure();
    void sendActivityInitData();
    void recvSlaveInitResp();
    void serializeGraphInit(MemoryBuffer &mb);

public:
    CMasterGraph(CJobChannel &jobChannel);
    ~CMasterGraph();

    virtual void init();
    virtual void executeSubGraph(size32_t parentExtractSz, const byte *parentExtract);
    CriticalSection &queryCreateLock() { return createdCrit; }
    void handleSlaveDone(unsigned node, MemoryBuffer &mb);
    bool serializeActivityInitData(unsigned slave, MemoryBuffer &mb, IThorActivityIterator &iter);
    void readActivityInitData(MemoryBuffer &mb, unsigned slave);
    bool deserializeStats(unsigned node, MemoryBuffer &mb);
    virtual void setComplete(bool tf=true);
    virtual bool prepare(size32_t parentExtractSz, const byte *parentExtract, bool checkDependencies, bool shortCircuit, bool async) override;
    virtual void execute(size32_t _parentExtractSz, const byte *parentExtract, bool checkDependencies, bool async) override;

    virtual bool preStart(size32_t parentExtractSz, const byte *parentExtract) override;
    virtual void start() override;
    virtual void done() override;
    virtual void reset() override;
    virtual void abort(IException *e) override;
    IThorResult *createResult(CActivityBase &activity, unsigned id, IThorGraphResults *results, IThorRowInterfaces *rowIf, ThorGraphResultType resultType, unsigned spillPriority=SPILL_PRIORITY_RESULT);
    IThorResult *createResult(CActivityBase &activity, unsigned id, IThorRowInterfaces *rowIf, ThorGraphResultType resultType, unsigned spillPriority=SPILL_PRIORITY_RESULT);
    IThorResult *createGraphLoopResult(CActivityBase &activity, IThorRowInterfaces *rowIf, ThorGraphResultType resultType, unsigned spillPriority=SPILL_PRIORITY_RESULT);

// IExceptionHandler
    virtual bool fireException(IException *e);
// IThorChildGraph impl.
    virtual IEclGraphResults *evaluate(unsigned _parentExtractSz, const byte *parentExtract);
};

class CSlaveMessageHandler : public CInterface, implements IThreaded
{
    CJobMaster &job;
    CThreaded threaded;
    bool stopped;
    mptag_t mptag;
    unsigned childGraphInitTimeout;
public:
    CSlaveMessageHandler(CJobMaster &job, mptag_t mptag);
    ~CSlaveMessageHandler();
    void stop();
    virtual void threadmain() override;
};

class graphmaster_decl CJobMaster : public CJobBase
{
    typedef CJobBase PARENT;

    Linked<IConstWorkUnit> workunit;
    Owned<IFatalHandler> fatalHandler;
    bool querySent, sendSo, spillsSaved;
    Int64Array nodeDiskUsage;
    bool nodeDiskUsageCached;
    StringArray createdFiles;
    Owned<CSlaveMessageHandler> slaveMsgHandler;
    SocketEndpoint agentEp;
    CriticalSection sendQueryCrit, spillCrit;

    void initNodeDUCache();

public:
    CJobMaster(IConstWorkUnit &workunit, const char *_graphName, ILoadedDllEntry *querySo, bool _sendSo, const SocketEndpoint &_agentEp);
    virtual void endJob() override;

    virtual void addChannel(IMPServer *mpServer);

    void registerFile(const char *logicalName, StringArray &clusters, unsigned usageCount=0, WUFileKind fileKind=WUFileStandard, bool temp=false);
    void deregisterFile(const char *logicalName, bool kept=false);
    const SocketEndpoint &queryAgentEp() const { return agentEp; }
    void broadcast(ICommunicator &comm, CMessageBuffer &msg, mptag_t mptag, unsigned timeout, const char *errorMsg, CReplyCancelHandler *msgHandler=NULL, bool sendOnly=false);
    IPropertyTree *prepareWorkUnitInfo();
    void sendQuery();
    void jobDone();
    void saveSpills();
    bool go();
    void pause(bool abort);

    virtual IConstWorkUnit &queryWorkUnit() const
    {
        CriticalBlock b(wuDirty);
        if (dirty)
        {
            dirty = false;
            workunit->forceReload();
        }
        return *workunit;
    }
    virtual void markWuDirty()
    {
        CriticalBlock b(wuDirty);
        dirty = true;
    }
    
// CJobBase impls.
    virtual mptag_t allocateMPTag();
    virtual void freeMPTag(mptag_t tag);
    virtual IGraphTempHandler *createTempHandler(bool errorOnMissing);

    CGraphTableCopy executed;
    CriticalSection exceptCrit;

    virtual __int64 getWorkUnitValueInt(const char *prop, __int64 defVal) const;
    virtual StringBuffer &getWorkUnitValue(const char *prop, StringBuffer &str) const;
    virtual bool getWorkUnitValueBool(const char *prop, bool defVal) const;

// IExceptionHandler
    virtual bool fireException(IException *e);

    virtual void addCreatedFile(const char *file);
    virtual __int64 addNodeDiskUsage(unsigned node, __int64 sz);

    __int64 queryNodeDiskUsage(unsigned node);
    void setNodeDiskUsage(unsigned node, __int64 sz);
    bool queryCreatedFile(const char *file);
};

class graphmaster_decl CJobMasterChannel : public CJobChannel
{
public:
    CJobMasterChannel(CJobBase &job, IMPServer *mpServer, unsigned channel);

    virtual CGraphBase *createGraph();
    virtual IBarrier *createBarrier(mptag_t tag);
// IExceptionHandler
    virtual bool fireException(IException *e) { return job.fireException(e); }
};



class graphmaster_decl CThorStats : public CInterface
{
protected:
    CJobBase &ctx;
    unsigned __int64 max, min, tot, avg;
    unsigned maxSkew, minSkew, minNode, maxNode;
    UInt64Array counts;
    StatisticKind kind;

public:
    IMPLEMENT_IINTERFACE;

    CThorStats(CJobBase &ctx, StatisticKind _kind);
    void reset();
    virtual void processInfo();

    unsigned __int64 queryTotal() { return tot; }
    unsigned __int64 queryAverage() { return avg; }
    unsigned __int64 queryMin() { return min; }
    unsigned __int64 queryMax() { return max; }
    unsigned queryMinSkew() { return minSkew; }
    unsigned queryMaxSkew() { return maxSkew; }
    unsigned queryMaxNode() { return maxNode; }
    unsigned queryMinNode() { return minNode; }

    void extract(unsigned node, const CRuntimeStatisticCollection & stats);
    void set(unsigned node, unsigned __int64 count);
    void getTotalStat(IStatisticGatherer & stats);
    void getStats(IStatisticGatherer & stats, bool suppressMinMaxWhenEqual);

protected:
    void processTotal();
    void calculateSkew();
    void tallyValue(unsigned __int64 value, unsigned node);
};

class graphmaster_decl CThorStatsCollection : public CInterface
{
public:
    CThorStatsCollection(CJobBase &ctx, const StatisticsMapping & _mapping) : mapping(_mapping)
    {
        unsigned num = mapping.numStatistics();
        stats = new Owned<CThorStats>[num];
        for (unsigned i=0; i < num; i++)
            stats[i].setown(new CThorStats(ctx, mapping.getKind(i)));
    }
    ~CThorStatsCollection()
    {
        delete [] stats;
    }

    void deserializeMerge(unsigned node, MemoryBuffer & mb)
    {
        CRuntimeStatisticCollection nodeStats(mapping);
        nodeStats.deserialize(mb);
        extract(node, nodeStats);
    }

    void extract(unsigned node, const CRuntimeStatisticCollection & source)
    {
        for (unsigned i=0; i < mapping.numStatistics(); i++)
            stats[i]->extract(node, source);
    }

    void getStats(IStatisticGatherer & result)
    {
        for (unsigned i=0; i < mapping.numStatistics(); i++)
        {
            stats[i]->getStats(result, false);
        }
    }

private:
    Owned<CThorStats> * stats;
    const StatisticsMapping & mapping;
};

class graphmaster_decl CTimingInfo : public CThorStats
{
public:
    CTimingInfo(CJobBase &ctx);
    void getStats(IStatisticGatherer & stats) { CThorStats::getStats(stats, false); }
};

class graphmaster_decl ProgressInfo : public CThorStats
{
    unsigned startcount, stopcount;
public:
    ProgressInfo(CJobBase &ctx);

    virtual void processInfo();
    void getStats(IStatisticGatherer & stats);
};
typedef CIArrayOf<ProgressInfo> ProgressInfoArray;

class graphmaster_decl CMasterActivity : public CActivityBase, implements IThreaded
{
    CThreaded threaded;
    bool asyncStart;
    MemoryBuffer *data;
    CriticalSection progressCrit;
    IArrayOf<IDistributedFile> readFiles;

protected:
    ProgressInfoArray progressInfo;
    CTimingInfo timingInfo;
    IBitSet *notedWarnings;

    void addReadFile(IDistributedFile *file, bool temp=false);
    IDistributedFile *queryReadFile(unsigned f);
    virtual void process() { }
public:
    IMPLEMENT_IINTERFACE_USING(CActivityBase)

    CMasterActivity(CGraphElementBase *container);
    ~CMasterActivity();

    virtual void deserializeStats(unsigned node, MemoryBuffer &mb);
    virtual void getActivityStats(IStatisticGatherer & stats);
    virtual void getEdgeStats(IStatisticGatherer & stats, unsigned idx);
    virtual void init();
    virtual void handleSlaveMessage(CMessageBuffer &msg) { }
    virtual void reset();
    virtual MemoryBuffer &queryInitializationData(unsigned slave) const;
    virtual MemoryBuffer &getInitializationData(unsigned slave, MemoryBuffer &dst) const;

    virtual void configure() { }
    virtual void serializeSlaveData(MemoryBuffer &dst, unsigned slave) { }
    virtual void slaveDone(size32_t slaveIdx, MemoryBuffer &mb) { }

    virtual void preStart(size32_t parentExtractSz, const byte *parentExtract);
    virtual void startProcess(bool async=true);
    virtual bool wait(unsigned timeout);
    virtual void done();
    virtual void kill();

// IExceptionHandler
    virtual bool fireException(IException *e);
// IThreaded
    virtual void threadmain() override;
};

class graphmaster_decl CMasterGraphElement : public CGraphElementBase
{
    bool initialized = false;
public:
    CMasterGraphElement(CGraphBase &owner, IPropertyTree &xgmml);
    void doCreateActivity(size32_t parentExtractSz=0, const byte *parentExtract=nullptr, MemoryBuffer *startCtx=nullptr);
    virtual bool checkUpdate();
    virtual void initActivity() override;
    virtual void reset() override;
    virtual void slaveDone(size32_t slaveIdx, MemoryBuffer &mb);
};

////////////////////////

class graphmaster_decl CThorResourceMaster : public CThorResourceBase
{
public:
    IMPLEMENT_IINTERFACE;

    CThorResourceMaster()
    {
    }
    ~CThorResourceMaster()
    {
    }
// IThorResource
};

#endif

