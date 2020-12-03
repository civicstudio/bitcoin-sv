#include "mining/journal_change_set.h"
#include "mempooltxdb.h"

#include "mempool_test_access.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <algorithm>
#include <random>
#include <vector>

namespace {
    mining::CJournalChangeSetPtr nullChangeSet{nullptr};

    std::vector<CTxMemPoolEntry> GetABunchOfEntries(int howMany)
    {
        TestMemPoolEntryHelper entry;
        std::vector<CTxMemPoolEntry> result;
        for (int i = 0; i < howMany; i++) {
            CMutableTransaction mtx;
            mtx.vin.resize(1);
            mtx.vin[0].scriptSig = CScript() << OP_11;
            mtx.vout.resize(1);
            mtx.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
            mtx.vout[0].nValue = Amount(33000LL + i);
            result.emplace_back(entry.FromTx(mtx));
        }
        return result;
    }
}

BOOST_FIXTURE_TEST_SUITE(mempooltxdb_tests, TestingSetup)
BOOST_AUTO_TEST_CASE(WriteToTxDB)
{
    const auto entries = GetABunchOfEntries(11);

    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    // Write the entries to the database.
    uint64_t totalSize = 0;
    for (const auto& e : entries)
    {
        totalSize += e.GetTxSize();
        BOOST_CHECK(txdb.AddTransactions({e.GetSharedTx()}));
    }
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), totalSize);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), entries.size());

    // Check that all transactions are in the database.
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(txdb.GetTransaction(e.GetTxId(), _));
    }
}

BOOST_AUTO_TEST_CASE(DoubleWriteToTxDB)
{
    const auto entries = GetABunchOfEntries(13);

    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    // Write the entries to the database.
    uint64_t totalSize = 0;
    for (const auto& e : entries)
    {
        totalSize += e.GetTxSize();
        BOOST_CHECK(txdb.AddTransactions({e.GetSharedTx()}));
    }
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), totalSize);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), entries.size());

    // Check that all transactions are in the database.
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(txdb.GetTransaction(e.GetTxId(), _));
    }

    // Write and check again.
    for (const auto& e : entries)
    {
        BOOST_CHECK(txdb.AddTransactions({e.GetSharedTx()}));
    }
    BOOST_WARN_EQUAL(txdb.GetDiskUsage(), totalSize);
    BOOST_WARN_EQUAL(txdb.GetTxCount(), entries.size());
    BOOST_CHECK_GE(txdb.GetDiskUsage(), totalSize);
    BOOST_CHECK_GE(txdb.GetTxCount(), entries.size());
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(txdb.GetTransaction(e.GetTxId(), _));
    }
}

BOOST_AUTO_TEST_CASE(DeleteFromTxDB)
{
    const auto entries = GetABunchOfEntries(17);

    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    // Write the entries to the database.
    uint64_t totalSize = 0;
    for (const auto& e : entries)
    {
        totalSize += e.GetTxSize();
        BOOST_CHECK(txdb.AddTransactions({e.GetSharedTx()}));
    }
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), totalSize);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), entries.size());

    // Remove transactions from the database one by one.
    for (const auto& e : entries)
    {
        BOOST_CHECK(txdb.RemoveTransactions({{e.GetTxId(), e.GetTxSize()}}));
    }
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(!txdb.GetTransaction(e.GetTxId(), _));
    }
}

BOOST_AUTO_TEST_CASE(BatchDeleteFromTxDB)
{
    const auto entries = GetABunchOfEntries(19);

    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    // Write the entries to the database.
    uint64_t totalSize = 0;
    std::vector<CMempoolTxDB::TxData> txdata;
    for (const auto& e : entries)
    {
        totalSize += e.GetTxSize();
        txdata.emplace_back(e.GetTxId(), e.GetTxSize());
        BOOST_CHECK(txdb.AddTransactions({e.GetSharedTx()}));
    }
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), totalSize);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), entries.size());

    // Remove all transactions from the database at once.
    BOOST_CHECK(txdb.RemoveTransactions(txdata));
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(!txdb.GetTransaction(e.GetTxId(), _));
    }
}

BOOST_AUTO_TEST_CASE(BadDeleteFromTxDB)
{
    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    // Remove nonexistent transactions.
    const auto e = GetABunchOfEntries(3);
    BOOST_CHECK(txdb.RemoveTransactions({
                {e[0].GetTxId(), e[0].GetTxSize()},
                {e[1].GetTxId(), e[1].GetTxSize()},
                {e[2].GetTxId(), e[2].GetTxSize()}
            }));
    BOOST_WARN_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_WARN_EQUAL(txdb.GetTxCount(), 0);
}

BOOST_AUTO_TEST_CASE(ClearTxDB)
{
    const auto entries = GetABunchOfEntries(23);

    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    // Write the entries to the database.
    uint64_t totalSize = 0;
    for (const auto& e : entries)
    {
        totalSize += e.GetTxSize();
        BOOST_CHECK(txdb.AddTransactions({e.GetSharedTx()}));
    }
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), totalSize);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), entries.size());

    // Clear the database and check that it's empty.
    txdb.ClearDatabase();
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(!txdb.GetTransaction(e.GetTxId(), _));
    }
}

BOOST_AUTO_TEST_CASE(GetContentsOfTxDB)
{
    const auto entries = GetABunchOfEntries(29);

    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    // Write the entries to the database.
    uint64_t totalSize = 0;
    for (const auto& e : entries)
    {
        totalSize += e.GetTxSize();
        BOOST_CHECK(txdb.AddTransactions({e.GetSharedTx()}));
    }
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), totalSize);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), entries.size());

    // Check that all transactions are in the database and only the ones we wrote.
    auto keys = txdb.GetKeys();
    BOOST_CHECK_EQUAL(keys.size(), 29);
    for (const auto& e : entries)
    {
        auto iter = keys.find(e.GetTxId());
        BOOST_WARN(iter != keys.end());
        if (iter != keys.end())
        {
            keys.erase(iter);
        }
    }
    // We should have removed all the keys in the loop.
    BOOST_CHECK_EQUAL(keys.size(), 0);
}

BOOST_AUTO_TEST_CASE(GetSetXrefKey)
{
    boost::uuids::random_generator gen;
    const auto uuid = gen();
    auto xref = decltype(uuid)();
    BOOST_CHECK_NE(to_string(uuid), to_string(xref));

    CMempoolTxDB txdb(10000);
    BOOST_CHECK(!txdb.GetXrefKey(xref));
    BOOST_CHECK(txdb.SetXrefKey(uuid));
    BOOST_CHECK(txdb.GetXrefKey(xref));
    BOOST_CHECK_EQUAL(to_string(uuid), to_string(xref));
}

BOOST_AUTO_TEST_CASE(RemoveXrefKey)
{
    boost::uuids::random_generator gen;
    const auto uuid = gen();
    auto xref = decltype(uuid)();

    CMempoolTxDB txdb(10000);
    BOOST_CHECK(!txdb.GetXrefKey(xref));
    BOOST_CHECK(txdb.SetXrefKey(uuid));
    BOOST_CHECK(txdb.GetXrefKey(xref));
    BOOST_CHECK(txdb.RemoveXrefKey());
    BOOST_CHECK(!txdb.GetXrefKey(xref));
}

BOOST_AUTO_TEST_CASE(AutoRemoveXrefKey)
{
    boost::uuids::random_generator gen;
    const auto uuid = gen();
    auto xref = decltype(uuid)();
    const auto entries = GetABunchOfEntries(1);
    const auto& e = entries[0];

    CMempoolTxDB txdb(10000);
    BOOST_CHECK(!txdb.GetXrefKey(xref));
    BOOST_CHECK(txdb.SetXrefKey(uuid));
    BOOST_CHECK(txdb.GetXrefKey(xref));
    txdb.AddTransactions({e.GetSharedTx()});
    BOOST_CHECK(!txdb.GetXrefKey(xref));

    BOOST_CHECK(txdb.SetXrefKey(uuid));
    BOOST_CHECK(txdb.GetXrefKey(xref));
    txdb.RemoveTransactions({{e.GetTxId(), e.GetTxSize()}});
    BOOST_CHECK(!txdb.GetXrefKey(xref));
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
}

BOOST_AUTO_TEST_CASE(BatchWriteWrite)
{
    const auto entries = GetABunchOfEntries(1);
    const auto& entry = entries[0];

    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    int counter = 0;
    const auto update = [&entry, &counter](const TxId& txid) {
        BOOST_CHECK_EQUAL(txid.ToString(), entry.GetTxId().ToString());
        ++counter;
    };

    CMempoolTxDB::Batch batch;
    batch.Add(entry.GetSharedTx(), update);
    batch.Add(entry.GetSharedTx(), update);
    BOOST_CHECK(txdb.Commit(batch));
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), entry.GetTxSize());
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 1);
    BOOST_CHECK_EQUAL(counter, 1);
}

BOOST_AUTO_TEST_CASE(BatchWriteRemove)
{
    const auto entries = GetABunchOfEntries(1);
    const auto& entry = entries[0];

    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    int counter = 0;
    const auto update = [&entry, &counter](const TxId& txid) {
        BOOST_CHECK_EQUAL(txid.ToString(), entry.GetTxId().ToString());
        ++counter;
    };

    CMempoolTxDB::Batch batch;
    batch.Add(entry.GetSharedTx(), update);
    batch.Remove(entry.GetTxId(), entry.GetTxSize());
    BOOST_CHECK(txdb.Commit(batch));
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);
    BOOST_CHECK_EQUAL(counter, 0);
}

BOOST_AUTO_TEST_CASE(BatchWriteRemoveWrite)
{
    const auto entries = GetABunchOfEntries(1);
    const auto& entry = entries[0];

    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    int counter = 0;
    const auto update = [&entry, &counter](const TxId& txid) {
        BOOST_CHECK_EQUAL(txid.ToString(), entry.GetTxId().ToString());
        ++counter;
    };

    CMempoolTxDB::Batch batch;
    batch.Add(entry.GetSharedTx(), update);
    batch.Remove(entry.GetTxId(), entry.GetTxSize());
    batch.Add(entry.GetSharedTx(), update);
    BOOST_CHECK(txdb.Commit(batch));
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), entry.GetTxSize());
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 1);
    BOOST_CHECK_EQUAL(counter, 1);
}

BOOST_AUTO_TEST_CASE(Write_BatchRemoveWrite)
{
    const auto entries = GetABunchOfEntries(1);
    const auto& entry = entries[0];

    CMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    BOOST_CHECK(txdb.AddTransactions({entry.GetSharedTx()}));
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), entry.GetTxSize());
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 1);

    int counter = 0;
    const auto update = [&entry, &counter](const TxId& txid) {
        BOOST_CHECK_EQUAL(txid.ToString(), entry.GetTxId().ToString());
        ++counter;
    };

    CMempoolTxDB::Batch batch;
    batch.Remove(entry.GetTxId(), entry.GetTxSize());
    batch.Add(entry.GetSharedTx(), update);
    BOOST_CHECK(txdb.Commit(batch));
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), entry.GetTxSize());
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 1);
    BOOST_CHECK_EQUAL(counter, 0);
}

BOOST_AUTO_TEST_CASE(AsyncWriteToTxDB)
{
    const auto entries = GetABunchOfEntries(11);

    CAsyncMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    // Write the entries to the database.
    uint64_t totalSize = 0;
    std::vector<CTransactionWrapperRef> wrappers;
    for (const auto& e : entries)
    {
        totalSize += e.GetTxSize();
        wrappers.emplace_back(CTestTxMemPoolEntry(const_cast<CTxMemPoolEntry&>(e)).Wrapper());
    }

    txdb.Add(std::move(wrappers));
    txdb.Sync();
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), totalSize);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), entries.size());

    // Check that all transactions are in the databas.
    const auto innerdb = txdb.GetDatabase();
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(innerdb->GetTransaction(e.GetTxId(), _));
    }
}

BOOST_AUTO_TEST_CASE(AsyncDeleteFromTxDB)
{
    const auto entries = GetABunchOfEntries(13);

    CAsyncMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    // Write the entries to the database.
    std::vector<CMempoolTxDB::TxData> txdata;
    std::vector<CTransactionWrapperRef> wrappers;
    for (const auto& e : entries)
    {
        txdata.emplace_back(e.GetTxId(), e.GetTxSize());
        wrappers.emplace_back(CTestTxMemPoolEntry(const_cast<CTxMemPoolEntry&>(e)).Wrapper());
    }
    txdb.Add(std::move(wrappers));

    // Remove all transactions from the database at once.
    txdb.Remove(std::move(txdata));
    txdb.Sync();
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);
    const auto innerdb = txdb.GetDatabase();
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(!innerdb->GetTransaction(e.GetTxId(), _));
    }
}

BOOST_AUTO_TEST_CASE(AsyncClearDB)
{
    const auto entries = GetABunchOfEntries(17);

    CAsyncMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    // Write the entries to the database.
    std::vector<CTransactionWrapperRef> wrappers;
    for (const auto& e : entries)
    {
        wrappers.emplace_back(CTestTxMemPoolEntry(const_cast<CTxMemPoolEntry&>(e)).Wrapper());
    }

    txdb.Add(std::move(wrappers));
    txdb.Clear();
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    const auto innerdb = txdb.GetDatabase();
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(!innerdb->GetTransaction(e.GetTxId(), _));
    }
}

BOOST_AUTO_TEST_CASE(AsyncMultiWriteCoalesce)
{
    const auto entries = GetABunchOfEntries(1223);

    CAsyncMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    for (const auto& e : entries)
    {
        txdb.Add({CTestTxMemPoolEntry(const_cast<CTxMemPoolEntry&>(e)).Wrapper()});
    }

    txdb.Sync();
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), entries.size());
    BOOST_CHECK_LT(txdb.GetWriteCount(), entries.size());
    BOOST_TEST_MESSAGE("AsyncMultiWriteCoalesce: " << txdb.GetWriteCount()
                       << " batch writes for " << entries.size() << " adds");

    const auto innerdb = txdb.GetDatabase();
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(innerdb->GetTransaction(e.GetTxId(), _));
    }
}

BOOST_AUTO_TEST_CASE(AsyncMultiWriteRemoveCoalesce)
{
    std::random_device rndev;
    std::mt19937 generator(rndev());

    auto entries = GetABunchOfEntries(541);
    const auto middle = entries.begin() + entries.size() / 2;

    CAsyncMempoolTxDB txdb(10000);
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);

    for (auto it = entries.begin(); it != middle; ++it)
    {
        txdb.Add({CTestTxMemPoolEntry(const_cast<CTxMemPoolEntry&>(*it)).Wrapper()});
    }
    std::shuffle(entries.begin(), middle, generator);
    for (auto it = entries.begin(); it != middle; ++it)
    {
        txdb.Remove({{it->GetTxId(), it->GetTxSize()}});
    }
    txdb.Sync();

    for (auto it = middle; it != entries.end(); ++it)
    {
        txdb.Add({CTestTxMemPoolEntry(const_cast<CTxMemPoolEntry&>(*it)).Wrapper()});
    }
    std::shuffle(middle, entries.end(), generator);
    for (auto it = middle; it != entries.end(); ++it)
    {
        txdb.Remove({{it->GetTxId(), it->GetTxSize()}});
    }
    txdb.Sync();

    BOOST_CHECK_EQUAL(txdb.GetTxCount(), 0);
    BOOST_CHECK_LT(txdb.GetWriteCount(), 2 * entries.size());
    BOOST_TEST_MESSAGE("AsyncMultiWriteRemoveCoalesce: " << txdb.GetWriteCount()
                       << " batch writes for " << entries.size() << " adds"
                       << " and " << entries.size() << " deletes");

    const auto innerdb = txdb.GetDatabase();
    for (const auto& e : entries)
    {
        CTransactionRef _;
        BOOST_CHECK(!innerdb->GetTransaction(e.GetTxId(), _));
    }
}

BOOST_AUTO_TEST_CASE(AsyncGetSetXrefKey)
{
    boost::uuids::random_generator gen;
    const auto uuid = gen();
    auto xref = decltype(uuid)();
    BOOST_CHECK_NE(to_string(uuid), to_string(xref));

    CAsyncMempoolTxDB txdb(10000);
    BOOST_CHECK(!txdb.GetXrefKey(xref));
    BOOST_CHECK(txdb.SetXrefKey(uuid));
    BOOST_CHECK(txdb.GetXrefKey(xref));
    BOOST_CHECK_EQUAL(to_string(uuid), to_string(xref));
}

BOOST_AUTO_TEST_CASE(AsyncRemoveXrefKey)
{
    boost::uuids::random_generator gen;
    const auto uuid = gen();
    auto xref = decltype(uuid)();

    CAsyncMempoolTxDB txdb(10000);
    BOOST_CHECK(!txdb.GetXrefKey(xref));
    BOOST_CHECK(txdb.SetXrefKey(uuid));
    BOOST_CHECK(txdb.GetXrefKey(xref));
    BOOST_CHECK(txdb.RemoveXrefKey());
    BOOST_CHECK(!txdb.GetXrefKey(xref));
}

BOOST_AUTO_TEST_CASE(AsyncAutoRemoveXrefKey)
{
    boost::uuids::random_generator gen;
    const auto uuid = gen();
    auto xref = decltype(uuid)();
    auto entries = GetABunchOfEntries(1);
    auto& e = entries[0];

    CAsyncMempoolTxDB txdb(10000);
    BOOST_CHECK(!txdb.GetXrefKey(xref));
    BOOST_CHECK(txdb.SetXrefKey(uuid));
    BOOST_CHECK(txdb.GetXrefKey(xref));
    txdb.Add({CTestTxMemPoolEntry(e).Wrapper()});
    BOOST_CHECK(!txdb.GetXrefKey(xref));

    BOOST_CHECK(txdb.SetXrefKey(uuid));
    BOOST_CHECK(txdb.GetXrefKey(xref));
    txdb.Remove({{e.GetTxId(), e.GetTxSize()}});
    BOOST_CHECK(!txdb.GetXrefKey(xref));
    BOOST_CHECK_EQUAL(txdb.GetDiskUsage(), 0);
}

BOOST_AUTO_TEST_CASE(SaveOnFullMempool)
{
    TestMemPoolEntryHelper entry;
    // Parent transaction with three children, and three grand-children:
    CMutableTransaction txParent;
    txParent.vin.resize(1);
    txParent.vin[0].scriptSig = CScript() << OP_11;
    txParent.vout.resize(3);
    for (int i = 0; i < 3; i++) {
        txParent.vout[i].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txParent.vout[i].nValue = Amount(33000LL);
    }
    CMutableTransaction txChild[3];
    for (int i = 0; i < 3; i++) {
        txChild[i].vin.resize(1);
        txChild[i].vin[0].scriptSig = CScript() << OP_11;
        txChild[i].vin[0].prevout = COutPoint(txParent.GetId(), i);
        txChild[i].vout.resize(1);
        txChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txChild[i].vout[0].nValue = Amount(11000LL);
    }
    CMutableTransaction txGrandChild[3];
    for (int i = 0; i < 3; i++) {
        txGrandChild[i].vin.resize(1);
        txGrandChild[i].vin[0].scriptSig = CScript() << OP_11;
        txGrandChild[i].vin[0].prevout = COutPoint(txChild[i].GetId(), 0);
        txGrandChild[i].vout.resize(1);
        txGrandChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txGrandChild[i].vout[0].nValue = Amount(11000LL);
    }

    CTxMemPool testPool;
    CTxMemPoolTestAccess testPoolAccess(testPool);

    // Nothing in pool, remove should do nothing:
    BOOST_CHECK_EQUAL(testPool.Size(), 0);
    testPool.SaveTxsToDisk(10000);
    testPoolAccess.SyncWithMempoolTxDB();
    BOOST_CHECK_EQUAL(testPool.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(testPool.GetDiskTxCount(), 0);
    BOOST_CHECK_EQUAL(testPool.Size(), 0);

    // Add transactions:
    testPool.AddUnchecked(txParent.GetId(), entry.FromTx(txParent), TxStorage::memory, nullChangeSet);
    for (int i = 0; i < 3; i++) {
        testPool.AddUnchecked(txChild[i].GetId(), entry.FromTx(txChild[i]), TxStorage::memory, nullChangeSet);
        testPool.AddUnchecked(txGrandChild[i].GetId(), entry.FromTx(txGrandChild[i]), TxStorage::memory, nullChangeSet);
    }

    // Saving transactions to disk doesn't change the mempool size:
    const auto poolSize = testPool.Size();
    testPool.SaveTxsToDisk(10000);
    testPoolAccess.SyncWithMempoolTxDB();
    BOOST_CHECK_EQUAL(testPool.Size(), poolSize);

    // But it does store something to disk:
    const auto diskUsage = testPool.GetDiskUsage();
    const auto txCount = testPool.GetDiskTxCount();
    BOOST_CHECK_GT(diskUsage, 0);
    BOOST_CHECK_GT(txCount, 0);
    BOOST_CHECK(testPoolAccess.CheckMempoolTxDB());

    // Check that all transactions have been saved to disk:
    uint64_t sizeTxsAdded = 0;
    uint64_t countTxsAdded = 0;
    for (const auto& entry : testPoolAccess.mapTx().get<entry_time>())
    {
        BOOST_CHECK(!entry.IsInMemory());
        sizeTxsAdded += entry.GetTxSize();
        ++countTxsAdded;
    }
    BOOST_CHECK_EQUAL(diskUsage, sizeTxsAdded);
    BOOST_CHECK_EQUAL(txCount, countTxsAdded);
    BOOST_CHECK(testPoolAccess.CheckMempoolTxDB());
}

BOOST_AUTO_TEST_CASE(RemoveFromDiskOnMempoolTrim)
{
    const auto entries = GetABunchOfEntries(6);

    CTxMemPool testPool;
    CTxMemPoolTestAccess testPoolAccess(testPool);

    // Add transactions:
    for (auto& entry : entries) {
        testPool.AddUnchecked(entry.GetTxId(), entry, TxStorage::memory, nullChangeSet);
    }

    // Saving transactions to disk doesn't change the mempool size:
    const auto poolSize = testPool.Size();
    BOOST_CHECK_EQUAL(poolSize, entries.size());
    testPool.SaveTxsToDisk(10000);
    testPoolAccess.SyncWithMempoolTxDB();
    BOOST_CHECK_EQUAL(testPool.Size(), poolSize);

    // But it does store something to disk:
    BOOST_CHECK_GT(testPool.GetDiskUsage(), 0);
    BOOST_CHECK_GT(testPool.GetDiskTxCount(), 0);
    BOOST_CHECK(testPoolAccess.CheckMempoolTxDB());

    // Trimming the mempool size should also remove transactions from disk:
    testPool.TrimToSize(0, nullChangeSet);
    testPoolAccess.SyncWithMempoolTxDB();
    BOOST_CHECK_EQUAL(testPool.Size(), 0);
    BOOST_CHECK_EQUAL(testPool.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(testPool.GetDiskTxCount(), 0);
    BOOST_CHECK(testPoolAccess.CheckMempoolTxDB());
}

BOOST_AUTO_TEST_CASE(CheckMempoolTxDB)
{
    constexpr auto numberOfEntries = 6;
    const auto entries = GetABunchOfEntries(numberOfEntries);

    CTxMemPool testPool;
    CTxMemPoolTestAccess testPoolAccess(testPool);
    testPoolAccess.OpenMempoolTxDB();

    // Add transactions to the database that are not in the mempool.
    std::vector<CTransactionWrapperRef> wrappers;
    for (const auto& entry : entries)
    {
        // Create a copy of the transaction wrapper because Add() marks them as saved.
        wrappers.emplace_back(std::make_shared<CTransactionWrapper>(*CTestTxMemPoolEntry(const_cast<CTxMemPoolEntry&>(entry)).Wrapper()));
    }
    testPoolAccess.mempoolTxDB()->Add(std::move(wrappers));
    testPoolAccess.SyncWithMempoolTxDB();
    BOOST_CHECK_EQUAL(testPool.Size(), 0);
    BOOST_CHECK_GT(testPool.GetDiskUsage(), 0);
    BOOST_CHECK_GT(testPool.GetDiskTxCount(), 0);
    BOOST_CHECK(!testPoolAccess.CheckMempoolTxDB());

    // Clearing the database should put everything right again.
    testPoolAccess.mempoolTxDB()->Clear();
    BOOST_CHECK_EQUAL(testPool.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(testPool.GetDiskTxCount(), 0);
    BOOST_CHECK(testPoolAccess.CheckMempoolTxDB());

    // Add transactions to the mempool and mark them saved without writing to disk.
    for (auto& entry : entries)
    {
        testPool.AddUnchecked(entry.GetTxId(), entry, TxStorage::memory, nullChangeSet);
        auto it = testPoolAccess.mapTx().find(entry.GetTxId());
        BOOST_REQUIRE(it != testPoolAccess.mapTx().end());
        CTestTxMemPoolEntry(const_cast<CTxMemPoolEntry&>(*it)).Wrapper()->UpdateTxMovedToDisk();
        BOOST_CHECK(entry.IsInMemory());
        BOOST_CHECK(!it->IsInMemory());
    }
    testPoolAccess.SyncWithMempoolTxDB();
    BOOST_CHECK_EQUAL(testPool.Size(), numberOfEntries);
    BOOST_CHECK_EQUAL(testPool.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(testPool.GetDiskTxCount(), 0);
    BOOST_CHECK(!testPoolAccess.CheckMempoolTxDB());

    // Clearing the mempool should put everything right again.
    testPool.Clear();
    BOOST_CHECK_EQUAL(testPool.Size(), 0);
    BOOST_CHECK_EQUAL(testPool.GetDiskUsage(), 0);
    BOOST_CHECK_EQUAL(testPool.GetDiskTxCount(), 0);
    BOOST_CHECK(testPoolAccess.CheckMempoolTxDB());
}
BOOST_AUTO_TEST_SUITE_END()
