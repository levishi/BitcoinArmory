////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "Blockchain.h"
#include "lmdb_wrapper.h"
#include "BDM_supportClasses.h"
#include "BlockDataMap.h"

#include <future>
#include <atomic>
#include <condition_variable>

#ifndef _BLOCKCHAINSCANNER_H
#define _BLOCKCHAINSCANNER_H

#define OffsetAndSize pair<size_t, size_t>

////////////////////////////////////////////////////////////////////////////////
struct BCTX
{
   const uint8_t* data_;
   const size_t size_;

   uint32_t version_;
   uint32_t lockTime_;

   vector<OffsetAndSize> txins_;
   vector<OffsetAndSize> txouts_;

   BCTX(const uint8_t* data, size_t size) :
      data_(data), size_(size)
   {}
};

////////////////////////////////////////////////////////////////////////////////
class BlockData
{
private:
   uint8_t* data_;
   size_t size_;

   vector<BCTX> txns_;
   
public:
   BlockData(void) {}

   void deserialize(const uint8_t* data, size_t size,
      const BlockHeader&);
};

////////////////////////////////////////////////////////////////////////////////
struct BlockDataLink
{
   BlockData blockdata_;
   shared_ptr<shared_future<BlockDataLink>> next_;
};

////////////////////////////////////////////////////////////////////////////////
struct BlockDataBatch
{
   const unsigned start_;
   const unsigned end_;

   shared_ptr<shared_future<BlockDataLink>> first_;

   shared_future<bool> doneScanningUtxos_;
   mutex mu_;
   condition_variable readThreadCV_;

   atomic<unsigned> highestProcessedHeight_;
   
   //keep a reference to the file mmaps used by this object since we don't copy 
   //the data, just point at it.
   map<unsigned, BlockFileMapPointer> fileMaps_;

   //only for addresses and utxos we track
   map<BinaryData, StoredTxOut> utxos_;
   map<BinaryData, StoredScriptHistory> ssh_;

   BlockDataBatch(unsigned start, unsigned end) : 
      start_(start), end_(end)
   {
      highestProcessedHeight_.store(start, memory_order_relaxed);
   }
};

////////////////////////////////////////////////////////////////////////////////
class BlockchainScanner
{
private:
   Blockchain* blockchain_;
   LMDBBlockDatabase* db_;
   shared_ptr<ScrAddrFilter> scrAddrFilter_;
   BlockDataLoader blockDataLoader_;

   const unsigned nBlockFilesPerBatch_ = 4;
   const unsigned nBlocksLookAhead_ = 10;
   const unsigned totalThreadCount_;

   BinaryData topScannedBlockHash_;

   //only for relevant utxos
   map<BinaryData, StoredTxOut> utxoMap_;

private:
   void readBlockData(shared_ptr<BlockDataBatch>);
   void scanBlockData(shared_ptr<BlockDataBatch>);

public:
   BlockchainScanner(Blockchain* bc, LMDBBlockDatabase* db,
      shared_ptr<ScrAddrFilter> saf,
      BlockFiles& bf) :
      blockchain_(bc), db_(db), scrAddrFilter_(saf),
      totalThreadCount_(thread::hardware_concurrency()),
      blockDataLoader_(bf.folderPath(), true, true)
   {}

   void scan(uint32_t startHeight);
   void updateSSH(void);
   
   const BinaryData& getTopScannedBlockHash(void) const
   {
      return topScannedBlockHash_;
   }
};

#endif