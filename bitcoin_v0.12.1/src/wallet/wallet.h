// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLET_H
#define BITCOIN_WALLET_WALLET_H

#include "amount.h"
#include "streams.h"
#include "tinyformat.h"
#include "ui_interface.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "wallet/crypter.h"
#include "wallet/wallet_ismine.h"
#include "wallet/walletdb.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/shared_ptr.hpp>

/**
 * Settings
 */
extern CFeeRate payTxFee;
extern CAmount maxTxFee;
extern unsigned int nTxConfirmTarget;
extern bool bSpendZeroConfChange;
extern bool fSendFreeTransactions;

static const unsigned int DEFAULT_KEYPOOL_SIZE = 100; // 密钥池默认大小
//! -paytxfee default
static const CAmount DEFAULT_TRANSACTION_FEE = 0;
//! -paytxfee will warn if called with a higher fee than this amount (in satoshis) per KB
static const CAmount nHighTransactionFeeWarning = 0.01 * COIN;
//! -fallbackfee default
static const CAmount DEFAULT_FALLBACK_FEE = 20000;
//! -mintxfee default
static const CAmount DEFAULT_TRANSACTION_MINFEE = 1000;
//! -maxtxfee default
static const CAmount DEFAULT_TRANSACTION_MAXFEE = 0.1 * COIN;
//! minimum change amount
static const CAmount MIN_CHANGE = CENT;
//! Default for -spendzeroconfchange
static const bool DEFAULT_SPEND_ZEROCONF_CHANGE = true;
//! Default for -sendfreetransactions // 默认通过 -sendfreetransactions 选项设置
static const bool DEFAULT_SEND_FREE_TRANSACTIONS = false; // 默认免费发送交易，默认关闭
//! -txconfirmtarget default
static const unsigned int DEFAULT_TX_CONFIRM_TARGET = 2;
//! -maxtxfee will warn if called with a higher fee than this amount (in satoshis)
static const CAmount nHighTransactionMaxFeeWarning = 100 * nHighTransactionFeeWarning;
//! Largest (in bytes) free transaction we're willing to create // 我们希望创建的最大（以字节为单位）免费交易
static const unsigned int MAX_FREE_TRANSACTION_CREATE_SIZE = 1000; // 创建的最大免费交易大小（1000B）
static const bool DEFAULT_WALLETBROADCAST = true; // 钱包交易广播，默认开启

class CAccountingEntry;
class CBlockIndex;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CTxMemPool;
class CWalletTx;

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_LATEST = 60000
};


/** A key pool entry */
class CKeyPool // 一个密钥池条目（保存公钥）
{
public:
    int64_t nTime; // 时间
    CPubKey vchPubKey; // 公钥

    CKeyPool();
    CKeyPool(const CPubKey& vchPubKeyIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
    }
};

/** Address book data */
class CAddressBookData // 地址簿数据
{
public:
    std::string name; // 所属账户名
    std::string purpose; // 用途 / 目的

    CAddressBookData()
    {
        purpose = "unknown";
    }

    typedef std::map<std::string, std::string> StringMap;
    StringMap destdata;
};

struct CRecipient // 接收者
{
    CScript scriptPubKey; // 公钥脚本
    CAmount nAmount; // 金额
    bool fSubtractFeeFromAmount; // 该金额是否减去交易费的标志
};

typedef std::map<std::string, std::string> mapValue_t;


static void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n"))
    {
        nOrderPos = -1; // TODO: calculate elsewhere
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry // 输出条目
{
    CTxDestination destination; // 交易目的地
    CAmount amount; // 金额
    int vout; // 输出索引
};

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction // 一个连接它到区块链的默克分支交易
{
private:
  /** Constant used in hashBlock to indicate tx has been abandoned */
    static const uint256 ABANDON_HASH; // 在块哈希中使用的用于表示交易已被抛弃的常量

public:
    uint256 hashBlock; // 块哈希

    /* An nIndex == -1 means that hashBlock (in nonzero) refers to the earliest
     * block in the chain we know this or any in-wallet dependency conflicts
     * with. Older clients interpret nIndex == -1 as unconfirmed for backward
     * compatibility.
     */ // nIndex 等于 -1 意味着块哈希（非零）指的是链上最早的块，我们知道这个或任何钱包内部依赖冲突。旧客户端把 nIndex 等于 -1 解释为未确认向后的兼容性。
    int nIndex;

    CMerkleTx()
    {
        Init();
    }

    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }

    void Init()
    {
        hashBlock = uint256();
        nIndex = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        std::vector<uint256> vMerkleBranch; // For compatibility with older versions.
        READWRITE(*(CTransaction*)this);
        nVersion = this->nVersion;
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    int SetMerkleBranch(const CBlock& block);

    /**
     * Return depth of transaction in blockchain:
     * <0  : conflicts with a transaction this deep in the blockchain
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */ // 返回交易所在区块在区块链上的深度：<0:与区块链中交易冲突；==0:在内存池中（未上链），等待被加入区块；>=1:主链上有很多区块
    int GetDepthInMainChain(const CBlockIndex* &pindexRet) const; // 获取该交易在主链上的深度
    int GetDepthInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet); }
    bool IsInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet) > 0; }
    int GetBlocksToMaturity() const; // 获取据成熟所需的区块数，101 - 当前所在区块链深度
    bool AcceptToMemoryPool(bool fLimitFree=true, bool fRejectAbsurdFee=true); // 把当前交易添加到内存池
    bool hashUnset() const { return (hashBlock.IsNull() || hashBlock == ABANDON_HASH); } // 哈希未设置（为空或已抛弃的哈希）
    bool isAbandoned() const { return (hashBlock == ABANDON_HASH); } // 该交易是否标记为已抛弃
    void setAbandoned() { hashBlock = ABANDON_HASH; } // 标记该交易为已抛弃
};

/** 
 * A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */ // 一系列只有所属者才关心的交易的附加信息。它包含全部需要链接回区块链的未记录的交易。
class CWalletTx : public CMerkleTx // 钱包交易
{
private:
    const CWallet* pwallet; // 钱包指针

public:
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived; //! time received by this node
    unsigned int nTimeSmart;
    char fFromMe; // 交易从自己发出的标志
    std::string strFromAccount; // 该交易是从哪个账户发送
    int64_t nOrderPos; //! position in ordered transaction list // 在有序的交易列表中的位置

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fImmatureCreditCached;
    mutable bool fAvailableCreditCached; // 可用余额缓存标识
    mutable bool fWatchDebitCached;
    mutable bool fWatchCreditCached;
    mutable bool fImmatureWatchCreditCached;
    mutable bool fAvailableWatchCreditCached;
    mutable bool fChangeCached;
    mutable CAmount nDebitCached;
    mutable CAmount nCreditCached;
    mutable CAmount nImmatureCreditCached;
    mutable CAmount nAvailableCreditCached; // 缓存的可用的（可信的）金额
    mutable CAmount nWatchDebitCached;
    mutable CAmount nWatchCreditCached;
    mutable CAmount nImmatureWatchCreditCached;
    mutable CAmount nAvailableWatchCreditCached;
    mutable CAmount nChangeCached;

    CWalletTx()
    {
        Init(NULL);
    }

    CWalletTx(const CWallet* pwalletIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CMerkleTx& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CTransaction& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        strFromAccount.clear();
        fDebitCached = false;
        fCreditCached = false;
        fImmatureCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fChangeCached = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nImmatureCreditCached = 0;
        nAvailableCreditCached = 0;
        nWatchDebitCached = 0;
        nWatchCreditCached = 0;
        nAvailableWatchCreditCached = 0;
        nImmatureWatchCreditCached = 0;
        nChangeCached = 0;
        nOrderPos = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (ser_action.ForRead())
            Init(NULL);
        char fSpent = false;

        if (!ser_action.ForRead())
        {
            mapValue["fromaccount"] = strFromAccount;

            WriteOrderPos(nOrderPos, mapValue);

            if (nTimeSmart)
                mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        READWRITE(*(CMerkleTx*)this);
        std::vector<CMerkleTx> vUnused; //! Used to be vtxPrev
        READWRITE(vUnused);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (ser_action.ForRead())
        {
            strFromAccount = mapValue["fromaccount"];

            ReadOrderPos(nOrderPos, mapValue);

            nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(mapValue["timesmart"]) : 0;
        }

        mapValue.erase("fromaccount");
        mapValue.erase("version");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
    }

    //! make sure balances are recalculated // 确保余额被重新计算
    void MarkDirty() // 标记已变动
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void BindWallet(CWallet *pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

    //! filter decides which addresses will count towards the debit // 过滤器决定哪些地址将计入借账
    CAmount GetDebit(const isminefilter& filter) const;
    CAmount GetCredit(const isminefilter& filter) const;
    CAmount GetImmatureCredit(bool fUseCache=true) const;
    CAmount GetAvailableCredit(bool fUseCache=true) const; // 获取可用的金额（默认使用缓存）
    CAmount GetImmatureWatchOnlyCredit(const bool& fUseCache=true) const;
    CAmount GetAvailableWatchOnlyCredit(const bool& fUseCache=true) const;
    CAmount GetChange() const;

    void GetAmounts(std::list<COutputEntry>& listReceived,
                    std::list<COutputEntry>& listSent, CAmount& nFee, std::string& strSentAccount, const isminefilter& filter) const;

    void GetAccountAmounts(const std::string& strAccount, CAmount& nReceived,
                           CAmount& nSent, CAmount& nFee, const isminefilter& filter) const;

    bool IsFromMe(const isminefilter& filter) const
    {
        return (GetDebit(filter) > 0);
    }

    // True if only scriptSigs are different
    bool IsEquivalentTo(const CWalletTx& tx) const;

    bool InMempool() const;
    bool IsTrusted() const; // 如果该钱包交易的所有输入均在内存池中，则为可信

    bool WriteToDisk(CWalletDB *pwalletdb);

    int64_t GetTxTime() const;
    int GetRequestCount() const;

    bool RelayWalletTransaction();

    std::set<uint256> GetConflicts() const;
};




class COutput // 输出
{
public:
    const CWalletTx *tx; // 钱包交易指针
    int i; // 输出索引
    int nDepth; // 所在交易的深度（确认数）
    bool fSpendable; // 是否可花费

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn, bool fSpendableIn)
    {
        tx = txIn; i = iIn; nDepth = nDepthIn; fSpendable = fSpendableIn;
    }

    std::string ToString() const;
};




/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey // 包含一个有期限的私钥以防该私钥不会被使用
{
public:
    CPrivKey vchPrivKey; // 私钥
    int64_t nTimeCreated; // 创建时间
    int64_t nTimeExpires; // 过期时间
    std::string strComment; // 备注
    //! todo: add something to note what created it (user, getnewaddress, change)
    //!   maybe should have a map<string, string> property map

    CWalletKey(int64_t nExpires=0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(LIMITED_STRING(strComment, 65536));
    }
};



/** 
 * A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */ // CWallet 是密钥库的扩展，可以维持一组交易和余额，并提供创建新交易的能力。
class CWallet : public CCryptoKeyStore, public CValidationInterface
{
private:
    /**
     * Select a set of coins such that nValueRet >= nTargetValue and at least
     * all coins from coinControl are selected; Never select unconfirmed coins
     * if they are not ours
     */
    bool SelectCoins(const CAmount& nTargetValue, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet, const CCoinControl *coinControl = NULL) const;

    CWalletDB *pwalletdbEncryption; // 钱包数据库对象指针

    //! the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion; // 当前的钱包版本：客户端低于该版本时不能加载钱包

    //! the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nWalletMaxVersion; // 钱包的最大格式版本：内存中的变量，指定钱包可能升级的版本

    int64_t nNextResend;
    int64_t nLastResend;
    bool fBroadcastTransactions; // 广播交易的标志

    /**
     * Used to keep track of spent outpoints, and
     * detect and report conflicts (double-spends or
     * mutated transactions where the mutant gets mined).
     */ // 用于跟踪花费输出点，并侦测和报告冲突（双花 或 挖矿突变导致的可变的交易）
    typedef std::multimap<COutPoint, uint256> TxSpends;
    TxSpends mapTxSpends; // 交易花费映射列表
    void AddToSpends(const COutPoint& outpoint, const uint256& wtxid);
    void AddToSpends(const uint256& wtxid);

    /* Mark a transaction (and its in-wallet descendants) as conflicting with a particular block. */ // 标记一笔交易（和其在钱包内的子交易）为与一个特殊块冲突
    void MarkConflicted(const uint256& hashBlock, const uint256& hashTx);

    void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>);

public:
    /*
     * Main wallet lock.
     * This lock protects all the fields added by CWallet
     *   except for:
     *      fFileBacked (immutable after instantiation)
     *      strWalletFile (immutable after instantiation)
     */
    mutable CCriticalSection cs_wallet; // 主钱包锁，保护除 CWallet 类中以下两个成员外添加到钱包的全部成员变量

    bool fFileBacked; // 文件是否已备份的标志
    std::string strWalletFile; // 钱包文件的文件名

    std::set<int64_t> setKeyPool; // 密钥池集合，用于记录密钥索引，从数字 1 开始递增
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata; // 密钥元数据映射列表

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys; // 主密钥映射
    unsigned int nMasterKeyMaxID; // 主密钥最大索引号

    CWallet()
    {
        SetNull();
    }

    CWallet(const std::string& strWalletFileIn)
    {
        SetNull(); // 初始化钱包

        strWalletFile = strWalletFileIn; // 设置钱包文件
        fFileBacked = true; // 文件备份标志置为 true
    }

    ~CWallet()
    {
        delete pwalletdbEncryption;
        pwalletdbEncryption = NULL;
    }

    void SetNull() // 钱包对象内存初始化
    {
        nWalletVersion = FEATURE_BASE;
        nWalletMaxVersion = FEATURE_BASE;
        fFileBacked = false;
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = NULL;
        nOrderPosNext = 0;
        nNextResend = 0;
        nLastResend = 0;
        nTimeFirstKey = 0;
        fBroadcastTransactions = false;
    }

    std::map<uint256, CWalletTx> mapWallet; // 钱包交易映射列表 <交易索引， 钱包交易>
    std::list<CAccountingEntry> laccentries; // 账户条目列表

    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;
    TxItems wtxOrdered; // 有序交易映射列表

    int64_t nOrderPosNext; // 下一条交易的序号
    std::map<uint256, int> mapRequestCount; // 消息请求（getdata）次数映射列表 <区块哈希，次数>

    std::map<CTxDestination, CAddressBookData> mapAddressBook; // 地址簿映射列表 <地址， 地址簿数据>

    CPubKey vchDefaultKey; // 默认公钥

    std::set<COutPoint> setLockedCoins; // 锁定的交易输出集合

    int64_t nTimeFirstKey; // 首个密钥创建时间，代表钱包的创建时间

    const CWalletTx* GetWalletTx(const uint256& hash) const;

    //! check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf) { AssertLockHeld(cs_wallet); return nWalletMaxVersion >= wf; } // 检查是否我们被允许升级（或已支持）到已知的特性

    /**
     * populate vCoins with vector of available COutputs.
     */
    void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed=true, const CCoinControl *coinControl = NULL, bool fIncludeZeroValue=false) const;

    /**
     * Shuffle and select coins until nTargetValue is reached while avoiding
     * small change; This method is stochastic for some inputs and upon
     * completion the coin set and corresponding actual target value is
     * assembled
     */
    bool SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet) const;

    bool IsSpent(const uint256& hash, unsigned int n) const;

    bool IsLockedCoin(uint256 hash, unsigned int n) const;
    void LockCoin(COutPoint& output); // 锁定指定交易输出
    void UnlockCoin(COutPoint& output); // 解锁指定交易输出
    void UnlockAllCoins(); // 解锁全部交易输出
    void ListLockedCoins(std::vector<COutPoint>& vOutpts); // 获取锁定的交易输出集合

    /**
     * keystore implementation
     * Generate a new key
     */ // 密钥库
    CPubKey GenerateNewKey(); // 生成一个新密钥
    //! Adds a key to the store, and saves it to disk.
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey); // 添加密钥到钱包，并本地化到磁盘
    //! Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key, const CPubKey &pubkey) { return CCryptoKeyStore::AddKeyPubKey(key, pubkey); } // 添加一个密钥到钱包内存，不本地化到磁盘（用于 LoadWallet）
    //! Load metadata (used by LoadWallet)
    bool LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &metadata);

    bool LoadMinVersion(int nVersion) { AssertLockHeld(cs_wallet); nWalletVersion = nVersion; nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion); return true; }

    //! Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    //! Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript);
    bool LoadCScript(const CScript& redeemScript);

    //! Adds a destination data tuple to the store, and saves it to disk
    bool AddDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Erases a destination data tuple in the store and on disk
    bool EraseDestData(const CTxDestination &dest, const std::string &key);
    //! Adds a destination data tuple to the store, without saving it to disk
    bool LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Look up a destination data tuple in the store, return true if found false otherwise
    bool GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const;

    //! Adds a watch-only address to the store, and saves it to disk.
    bool AddWatchOnly(const CScript &dest);
    bool RemoveWatchOnly(const CScript &dest);
    //! Adds a watch-only address to the store, without saving it to disk (used by LoadWallet)
    bool LoadWatchOnly(const CScript &dest);

    bool Unlock(const SecureString& strWalletPassphrase);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase); // 改变钱包密码
    bool EncryptWallet(const SecureString& strWalletPassphrase); // 使用用户指定密码加密钱包

    void GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const;

    /** 
     * Increment the next transaction order id
     * @return next transaction order id
     */ // 增加下一条交易序号，返回下一个交易的序号
    int64_t IncOrderPosNext(CWalletDB *pwalletdb = NULL);

    void MarkDirty(); // 标记已变动
    bool AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet, CWalletDB* pwalletdb);
    void SyncTransaction(const CTransaction& tx, const CBlock* pblock);
    bool AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate);
    int ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false); // 从指定区块开始扫描钱包交易
    void ReacceptWalletTransactions(); // 再次接受钱包交易，把交易放入内存池
    void ResendWalletTransactions(int64_t nBestBlockTime);
    std::vector<uint256> ResendWalletTransactionsBefore(int64_t nTime); // 重新发送某时间点前的钱包交易
    CAmount GetBalance() const; // 获取钱包余额
    CAmount GetUnconfirmedBalance() const; // 获取钱包内未确认的余额
    CAmount GetImmatureBalance() const;
    CAmount GetWatchOnlyBalance() const;
    CAmount GetUnconfirmedWatchOnlyBalance() const;
    CAmount GetImmatureWatchOnlyBalance() const;

    /**
     * Insert additional inputs into the transaction by
     * calling CreateTransaction();
     */ // 通过调用 CreateTransaction() 插入额外的输入到交易中；
    bool FundTransaction(CMutableTransaction& tx, CAmount& nFeeRet, int& nChangePosRet, std::string& strFailReason, bool includeWatching);

    /**
     * Create a new transaction paying the recipients with a set of coins
     * selected by SelectCoins(); Also create the change output, when needed
     */ // 通过 SelectCoins() 筛选的一组金额创建一笔支付给接收者新交易；在需要时也创建找零输出。
    bool CreateTransaction(const std::vector<CRecipient>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, int& nChangePosRet,
                           std::string& strFailReason, const CCoinControl *coinControl = NULL, bool sign = true);
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey); // 提交交易

    bool AddAccountingEntry(const CAccountingEntry&, CWalletDB & pwalletdb); // 添加账户条目到钱包数据库

    static CFeeRate minTxFee; // 最小交易费
    static CFeeRate fallbackFee; // 撤销交易费
    /**
     * Estimate the minimum fee considering user set parameters
     * and the required fee
     */ // 考虑用户设置参数和所需费用，估算最低交易费
    static CAmount GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool);
    /**
     * Return the minimum required fee taking into account the
     * floating relay fee and user set minimum transaction fee
     */ // 考虑到浮动中继费和用户设置的最低交易费，返回所需最低交易费
    static CAmount GetRequiredFee(unsigned int nTxBytes);

    bool NewKeyPool(); // 标记旧密钥池的密钥位已使用，并生成全部新密钥
    bool TopUpKeyPool(unsigned int kpSize = 0); // 填充满密钥池
    void ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool);
    void KeepKey(int64_t nIndex); // 从密钥池中移除指定索引的密钥
    void ReturnKey(int64_t nIndex);
    bool GetKeyFromPool(CPubKey &key); // 从密钥池中获取一个密钥的公钥
    int64_t GetOldestKeyPoolTime();
    void GetAllReserveKeys(std::set<CKeyID>& setAddress) const; // 获取密钥池中全部预创建的密钥

    std::set< std::set<CTxDestination> > GetAddressGroupings(); // 获取地址分组集合
    std::map<CTxDestination, CAmount> GetAddressBalances(); // 获取地址余额映射列表

    std::set<CTxDestination> GetAccountAddresses(const std::string& strAccount) const; // 根据指定的账户获取相关联的地址集

    isminetype IsMine(const CTxIn& txin) const; // 交易输入是否属于本地钱包
    CAmount GetDebit(const CTxIn& txin, const isminefilter& filter) const;
    isminetype IsMine(const CTxOut& txout) const; // 交易输出是否属于本地钱包
    CAmount GetCredit(const CTxOut& txout, const isminefilter& filter) const;
    bool IsChange(const CTxOut& txout) const;
    CAmount GetChange(const CTxOut& txout) const;
    bool IsMine(const CTransaction& tx) const;
    /** should probably be renamed to IsRelevantToMe */
    bool IsFromMe(const CTransaction& tx) const;
    CAmount GetDebit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetCredit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetChange(const CTransaction& tx) const;
    void SetBestChain(const CBlockLocator& loc);

    DBErrors LoadWallet(bool& fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);

    bool SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& purpose); // 设置地址簿（地址，所属账户，地址用途）

    bool DelAddressBook(const CTxDestination& address);

    void UpdatedTransaction(const uint256 &hashTx);

    void Inventory(const uint256 &hash) // 增加库存（指定区块的 getdata 请求次数）
    {
        {
            LOCK(cs_wallet);
            std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
            if (mi != mapRequestCount.end())
                (*mi).second++;
        }
    }

    void GetScriptForMining(boost::shared_ptr<CReserveScript> &script);
    void ResetRequestCount(const uint256 &hash) // 重置区块哈希对应的 getdata 请求次数为 0
    {
        LOCK(cs_wallet);
        mapRequestCount[hash] = 0;
    };
    
    unsigned int GetKeyPoolSize() // 获取密钥池大小
    {
        AssertLockHeld(cs_wallet); // setKeyPool
        return setKeyPool.size(); // 返回密钥池索引集合的大小
    }

    bool SetDefaultKey(const CPubKey &vchPubKey);

    //! signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = NULL, bool fExplicit = false);

    //! change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

    //! get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion() { LOCK(cs_wallet); return nWalletVersion; }

    //! Get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> GetConflicts(const uint256& txid) const;

    //! Flush wallet (bitdb flush) // 刷新钱包（数据库刷新）
    void Flush(bool shutdown=false);

    //! Verify the wallet database and perform salvage if required // 验证钱包数据库，若需要则实施挽救
    static bool Verify(const std::string& walletFile, std::string& warningString, std::string& errorString);
    
    /** 
     * Address book entry changed.
     * @note called with lock cs_wallet held.
     */ // 地址簿条目改变。注：持有 cs_wallet 锁调用。
    boost::signals2::signal<void (CWallet *wallet, const CTxDestination
            &address, const std::string &label, bool isMine,
            const std::string &purpose,
            ChangeType status)> NotifyAddressBookChanged;

    /** 
     * Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */ // 钱包交易添加，移除和更新时。
    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashTx,
            ChangeType status)> NotifyTransactionChanged;

    /** Show progress e.g. for rescan */ // 显示进度，例如：再扫描
    boost::signals2::signal<void (const std::string &title, int nProgress)> ShowProgress;

    /** Watch-only address added */ // 添加 Watch-only 地址
    boost::signals2::signal<void (bool fHaveWatchOnly)> NotifyWatchonlyChanged;

    /** Inquire whether this wallet broadcasts transactions. */ // 查询该钱包是否广播交易。
    bool GetBroadcastTransactions() const { return fBroadcastTransactions; } // 返回广播交易标志
    /** Set whether this wallet broadcasts transactions. */ // 设置该钱包是否广播交易。
    void SetBroadcastTransactions(bool broadcast) { fBroadcastTransactions = broadcast; }

    /* Mark a transaction (and it in-wallet descendants) as abandoned so its inputs may be respent. */
    bool AbandonTransaction(const uint256& hashTx); // 标记一笔交易（及其钱包后裔）为以抛弃，以至于它的输入被重新关注
};

/** A key allocated from the key pool. */ // 一个从密钥池分配的密钥。
class CReserveKey : public CReserveScript
{
protected:
    CWallet* pwallet; // 钱包指针，指向主钱包
    int64_t nIndex; // 密钥池中密钥的索引，初始化为 -1
    CPubKey vchPubKey; // 对应公钥
public:
    CReserveKey(CWallet* pwalletIn)
    {
        nIndex = -1;
        pwallet = pwalletIn; // 主钱包
    }

    ~CReserveKey()
    {
        ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey); // 从密钥池中获取一个公钥
    void KeepKey(); // 从密钥池中移除密钥，并清空数据成员 vchPubKey
    void KeepScript() { KeepKey(); }
};


/** 
 * Account information.
 * Stored in wallet with key "acc"+string account name.
 */ // 账户信息。以 "acc"+账户名 为关键字存储在钱包中。
class CAccount
{
public:
    CPubKey vchPubKey; // 公钥

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey = CPubKey();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
    }
};



/** 
 * Internal transfers.
 * Database key is acentry<account><counter>.
 */ // 内部转账。数据库关键字是<账户><计数器>。
class CAccountingEntry // 账户条目类
{
public:
    std::string strAccount; // 账户名（借出、贷入）
    CAmount nCreditDebit; // 贷入借出金额
    int64_t nTime; // 转账时间（转出、到账）
    std::string strOtherAccount; // 对方账户名
    std::string strComment; // 备注信息
    mapValue_t mapValue;
    int64_t nOrderPos;  //! position in ordered transaction list // 在有序交易清单中的位置
    uint64_t nEntryNo;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
        nEntryNo = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        //! Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(LIMITED_STRING(strOtherAccount, 65536));

        if (!ser_action.ForRead())
        {
            WriteOrderPos(nOrderPos, mapValue);

            if (!(mapValue.empty() && _ssExtra.empty()))
            {
                CDataStream ss(nType, nVersion);
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                strComment.append(ss.str());
            }
        }

        READWRITE(LIMITED_STRING(strComment, 65536));

        size_t nSepPos = strComment.find("\0", 0, 1);
        if (ser_action.ForRead())
        {
            mapValue.clear();
            if (std::string::npos != nSepPos)
            {
                CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()), nType, nVersion);
                ss >> mapValue;
                _ssExtra = std::vector<char>(ss.begin(), ss.end());
            }
            ReadOrderPos(nOrderPos, mapValue);
        }
        if (std::string::npos != nSepPos)
            strComment.erase(nSepPos);

        mapValue.erase("n");
    }

private:
    std::vector<char> _ssExtra;
};

#endif // BITCOIN_WALLET_WALLET_H
