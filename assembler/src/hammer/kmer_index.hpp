//***************************************************************************
//* Copyright (c) 2011-2012 Saint-Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//****************************************************************************

#ifndef _HAMMER_KMERINDEX_HPP_
#define _HAMMER_KMERINDEX_HPP_

#include "mph_index/mph_index.h"
#include "mmapped_reader.hpp"
#include "mmapped_writer.hpp"
#include "pointer_iterator.hpp"

#include "openmp_wrapper.h"

#include "logger/logger.hpp"
#include "path_helper.hpp"

#include <boost/lexical_cast.hpp>

#include <algorithm>
#ifdef USE_GLIBCXX_PARALLEL
#include <parallel/algorithm>
#endif
#include <vector>

#include <cmath>

template<class Seq>
class KMerIndexBuilder;

template<class Seq>
class KMerIndex {
 public:
  typedef typename Seq::hash hash_function;

 private:
  typedef typename MMappedRecordArrayReader<typename Seq::DataType>::iterator::value_type KMerRawData;

  // This is really the most fragile part of the whole story.  Basically, we're
  // building the PHM with "raw" data, but query the PHM with real Key's!  We're
  // relying that hashes are exactly the same in both cases (thus - two almost
  // equal implementations!).
  struct seeded_hash_function {
    uint32_t operator()(const KMerRawData &k, uint32_t seed) const {
      uint32_t h;
      size_t h0 = hash_function()(k.ptr, k.size);
      MurmurHash3_x86_32(reinterpret_cast<const void*>(&h0), sizeof(h0), seed, &h);
      return h;
    }

    uint32_t operator()(const Seq &k, uint32_t seed) const {
      uint32_t h;
      size_t h0 = hash_function()(k);
      MurmurHash3_x86_32(reinterpret_cast<const void*>(&h0), sizeof(h0), seed, &h);
      return h;
    }

    cxxmph::h128 hash128(const KMerRawData &k, uint32_t seed) const {
      cxxmph::h128 h;
      size_t h0 = hash_function()(k.ptr, k.size);
      MurmurHash3_x64_128(reinterpret_cast<const void*>(&h0), sizeof(h0), seed, &h);
      return h;
    }

    cxxmph::h128 hash128(const Seq &k, uint32_t seed) const {
      cxxmph::h128 h;
      size_t h0 = hash_function()(k);
      MurmurHash3_x64_128(reinterpret_cast<const void*>(&h0), sizeof(h0), seed, &h);
      return h;
    }
  };

  typedef cxxmph::SimpleMPHIndex<Seq, seeded_hash_function> KMerDataIndex;

 public:
  KMerIndex():index_(NULL), num_buckets_(0) {}

  KMerIndex(const KMerIndex&) = delete;
  KMerIndex& operator=(const KMerIndex&) = delete;

  ~KMerIndex() { clear(); }

  void clear() {
    num_buckets_ = 0;
    bucket_starts_.clear();

    for (size_t i = 0; i < num_buckets_; ++i) {
      index_[i].clear();
    }

    delete[] index_;
  }

  size_t mem_size() {
    size_t sz = 0;
    for (size_t i = 0; i < num_buckets_; ++i)
      sz += index_[i].mem_size();

    return sz;
  }

  size_t seq_idx(Seq s) const {
    size_t bucket = seq_bucket(s);

    return bucket_starts_[bucket] + index_[bucket].index(s);
  }

  template<class Writer>
  void serialize(Writer &os) const {
    os.write((char*)&num_buckets_, sizeof(num_buckets_));
    for (size_t i = 0; i < num_buckets_; ++i)
      index_[i].serialize(os);
    os.write((char*)&bucket_starts_[0], (num_buckets_ + 1) * sizeof(bucket_starts_[0]));
  }

  template<class Reader>
  void deserialize(Reader &is) {
    clear();

    is.read((char*)&num_buckets_, sizeof(num_buckets_));

    index_ = new KMerDataIndex[num_buckets_];
    for (size_t i = 0; i < num_buckets_; ++i)
      index_[i].deserialize(is);

    bucket_starts_.resize(num_buckets_ + 1);
    is.read((char*)&bucket_starts_[0], (num_buckets_ + 1) * sizeof(bucket_starts_[0]));
  }

 private:
  KMerDataIndex *index_;

  size_t num_buckets_;
  std::vector<size_t> bucket_starts_;

  size_t seq_bucket(Seq s) const { return hash_function()(s) % num_buckets_; }

  friend class KMerIndexBuilder<Seq>;
};

class KMerSplitter {
 public:
  KMerSplitter(std::string &work_dir, unsigned num_files)
      : work_dir_(work_dir), num_files_(num_files) {}

  virtual path::files_t Split() = 0;

 protected:
  std::string work_dir_;
  unsigned num_files_;

  std::string GetRawKMersFname(unsigned suffix) const {
    return path::append_path(work_dir_, "kmers.raw." + boost::lexical_cast<std::string>(suffix));
  }

  DECL_LOGGER("K-mer Splitting");
};

template<class Seq>
class KMerIndexBuilder {
  std::string work_dir_;

 public:
  KMerIndexBuilder(const std::string &workdir)
      : work_dir_(workdir) {}
  size_t BuildIndex(KMerIndex<Seq> &out, KMerSplitter &splitter);

 private:
  size_t MergeKMers(const std::string &ifname, const std::string &ofname);
  std::string GetUniqueKMersFname(unsigned suffix) const {
    return path::append_path(work_dir_, "kmers.unique." + boost::lexical_cast<std::string>(suffix));
  }

  DECL_LOGGER("K-mer Index Building");
};

// FIXME: Change DataSize to runtime K
template<class Seq>
size_t KMerIndexBuilder<Seq>::MergeKMers(const std::string &ifname, const std::string &ofname) {
  MMappedRecordArrayReader<typename Seq::DataType> ins(ifname, Seq::DataSize, /* unlink */ true, -1ULL);
#ifdef USE_GLIBCXX_PARALLEL
  // Explicitly force a call to parallel sort routine.
  __gnu_parallel::sort(ins.begin(), ins.end());
#else
  std::sort(ins.begin(), ins.end());
#endif
  INFO("Sorting done, starting unification.");

  // FIXME: Use something like parallel version of unique_copy but with explicit
  // resizing.
  auto it = std::unique(ins.begin(), ins.end(),
                        typename array_ref<typename Seq::DataType>::equal_to());

  MMappedRecordArrayWriter<typename Seq::DataType> os(ofname, Seq::DataSize);
  os.resize((it - ins.begin()) * Seq::DataSize);
  std::copy(ins.begin(), it, os.begin());

  return it - ins.begin();
}

template<class Seq>
size_t KMerIndexBuilder<Seq>::BuildIndex(KMerIndex<Seq> &index, KMerSplitter &splitter) {
  index.clear();

  INFO("Building kmer index");

  // Split k-mers into buckets.
  path::files_t buckets = splitter.Split();
  unsigned num_buckets = buckets.size();

  INFO("Starting k-mer counting.");
  size_t sz = 0;
  index.bucket_starts_.push_back(sz);
  for (unsigned iFile = 0; iFile < num_buckets; ++iFile) {
    INFO("Processing file " << iFile);
    sz += MergeKMers(buckets[iFile], GetUniqueKMersFname(iFile));
    index.bucket_starts_.push_back(sz);
  }
  INFO("K-mer counting done. There are " << sz << " kmers in total. ");

  index.num_buckets_ = num_buckets;
  index.index_ = new typename KMerIndex<Seq>::KMerDataIndex[num_buckets];

  INFO("Building perfect hash indices");
# pragma omp parallel for shared(index)
  for (unsigned iFile = 0; iFile < num_buckets; ++iFile) {
    typename KMerIndex<Seq>::KMerDataIndex &data_index = index.index_[iFile];
    MMappedRecordArrayReader<typename Seq::DataType> ins(GetUniqueKMersFname(iFile), Seq::DataSize, /* unlink */ true, -1ULL);
    if (!data_index.Reset(ins.begin(), ins.end(), ins.end() - ins.begin())) {
      INFO("Something went really wrong (read = this should not happen). Try to restart and see if the problem will be fixed.");
      exit(-1);
    }
  }

  INFO("Index built. Total " << index.mem_size() << " bytes occupied.");

  return sz;
}

#endif // _HAMMER_KMERINDEX_HPP_
