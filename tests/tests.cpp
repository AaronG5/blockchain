#include <cstdint>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/headers/blockchain.hpp"
#include "../src/headers/helpers.hpp"

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { \
   ++checks; \
   if(!(cond)) { ++failures; std::cout << "   FAIL line " << __LINE__ << ": " #cond "\n"; } \
} while(0)

static bool contains(const std::string& text, const std::string& part) {
   return text.find(part) != std::string::npos;
}

// ---- Test setup ----

const std::string A = hash::hashFunction("user A");
const std::string B = hash::hashFunction("user B");
const std::string C = hash::hashFunction("user C");
const std::string NOBODY = hash::hashFunction("not a user");

const Transaction FUNDING({}, {{A, 100}, {B, 50}}, 0);
const Input A_COINS{FUNDING.getId(), 0};   // A's 100
const Input B_COINS{FUNDING.getId(), 1};   // B's 50

// Chain with a mined genesis block, ready for block 1
static Blockchain makeChain() {
   Blockchain chain(State(FUNDING), {A, B, C}, 3);
   Block genesis = chain.createBlock({});
   mine(genesis);
   std::string reason;
   chain.addBlock(genesis, reason);
   return chain;
}

static Block minedBlock(const Blockchain& chain, const std::vector<Transaction>& txs) {
   Block block = chain.createBlock(txs);
   mine(block);
   return block;
}

// Checks that a rejected block left everything as it was. `size` counts genesis
static void checkUnchanged(const Blockchain& chain, int size, std::uint64_t balanceA, std::uint64_t balanceB) {
   CHECK(chain.size() == size);
   CHECK(chain.getConfirmed().balanceOf(A) == balanceA);
   CHECK(chain.getConfirmed().balanceOf(B) == balanceB);
   CHECK(chain.getConfirmed().total() == 150);
}

// ---- Tests ----

// Root is built from IDs decoded to bytes, a lone last node is paired with itself
void merkleTree() {
   std::vector<std::string> ids;
   for(int i = 0; i < 5; ++i) ids.push_back(hash::hashFunction("tx " + std::to_string(i)));

   auto pair = [](const std::string& left, const std::string& right) {
      return hash::hashFunction(Block::hexToBytes(left) + Block::hexToBytes(right));
   };

   CHECK(Block::computeTxRoot(std::vector<std::string>{}) == hash::hashFunction(""));
   CHECK(Block::computeTxRoot({ids[0]}) == ids[0]);
   CHECK(Block::computeTxRoot({ids[0], ids[1]}) == pair(ids[0], ids[1]));
   CHECK(Block::computeTxRoot({ids[0], ids[1], ids[2]}) == pair(pair(ids[0], ids[1]), pair(ids[2], ids[2])));

   const std::string left = pair(pair(ids[0], ids[1]), pair(ids[2], ids[3]));
   const std::string right = pair(pair(ids[4], ids[4]), pair(ids[4], ids[4]));
   CHECK(Block::computeTxRoot(ids) == pair(left, right));

   // Swapping the order or changing one transaction (so its ID changes) changes the root
   CHECK(Block::computeTxRoot({ids[1], ids[0], ids[2]}) != Block::computeTxRoot({ids[0], ids[1], ids[2]}));
   const Transaction tx({A_COINS}, {{B, 70}, {A, 30}}, 1);
   const Transaction changed({A_COINS}, {{B, 71}, {A, 29}}, 1);
   CHECK(Block::computeTxRoot({tx.getId(), ids[1]}) != Block::computeTxRoot({changed.getId(), ids[1]}));
}

// A valid payment in a block smaller than 100 updates balances once and keeps the total
void validPayment() {
   Blockchain chain = makeChain();
   const Transaction pay({A_COINS}, {{B, 70}, {A, 30}}, 1);

   std::string reason;
   CHECK(chain.addBlock(minedBlock(chain, {pay}), reason));
   CHECK(chain.size() == 2);
   CHECK(chain.isIncluded(pay.getId()));
   CHECK(chain.getConfirmed().balanceOf(A) == 30);
   CHECK(chain.getConfirmed().balanceOf(B) == 120);
   CHECK(chain.getConfirmed().total() == chain.getInitial().total());
}

// Pool entry rejects broken transactions and says why
void poolRejectsBadTransactions() {
   const Blockchain chain = makeChain();
   const Transaction good({A_COINS}, {{B, 70}, {A, 30}}, 1);
   const Transaction unknownReceiver({A_COINS}, {{NOBODY, 70}, {A, 30}}, 2);
   const Transaction zeroAmount({A_COINS}, {{B, 100}, {A, 0}}, 3);
   const Transaction tamperedId({A_COINS}, {{B, 70}, {A, 30}}, 4, good.getId());

   std::vector<Rejected> rejected;
   const std::vector<Transaction> pool = filterPool({good, unknownReceiver, zeroAmount, tamperedId, good}, chain, rejected);

   CHECK(pool.size() == 1);
   CHECK(rejected.size() == 4);
   if(rejected.size() == 4) {
      CHECK(rejected[0].reason == "unknown receiver");
      CHECK(rejected[1].reason == "zero amount output");
      CHECK(rejected[2].reason == "invalid id");
      CHECK(rejected[3].reason == "id already in pool");
   }
}

// Two payments from A's single 100 coin (70 and 60) can't both be accepted
void overspending() {
   Blockchain chain = makeChain();
   const Transaction first({A_COINS}, {{B, 70}, {A, 30}}, 1);
   const Transaction second({A_COINS}, {{C, 60}, {A, 40}}, 2);
   const Transaction tooMuch({A_COINS}, {{B, 150}}, 3);
   std::string reason;

   // Paying more than the inputs hold
   CHECK(!chain.getConfirmed().canApply(tooMuch, reason));

   // Selection takes only one of them, and the real state stays as it was
   std::vector<Transaction> pool{first, second};
   Random rng(1);
   CHECK(selectTransactions(pool, chain.getConfirmed(), rng).size() == 1);
   checkUnchanged(chain, 1, 100, 50);

   // A block with both is rejected as a whole
   CHECK(!chain.addBlock(minedBlock(chain, {first, second}), reason));
   CHECK(contains(reason, "unknown or spent output"));
   checkUnchanged(chain, 1, 100, 50);
}

// The same transaction is never counted twice
void duplicatesAndReplays() {
   Blockchain chain = makeChain();
   const Transaction pay({A_COINS}, {{B, 70}, {A, 30}}, 1);
   std::string reason;

   CHECK(!chain.addBlock(minedBlock(chain, {pay, pay}), reason));
   CHECK(contains(reason, "duplicate in block"));
   checkUnchanged(chain, 1, 100, 50);

   CHECK(chain.addBlock(minedBlock(chain, {pay}), reason));

   // Replaying the confirmed payment in a later block
   CHECK(!chain.addBlock(minedBlock(chain, {pay}), reason));
   CHECK(contains(reason, "already in chain"));
   checkUnchanged(chain, 2, 30, 120);

   // ... or sending it to the pool again
   std::vector<Rejected> rejected;
   CHECK(filterPool({pay}, chain, rejected).empty());
   CHECK(rejected.size() == 1 && rejected[0].reason == "id already in chain");
}

// Broken blocks are rejected and leave the chain and balances unchanged
void tamperedBlocks() {
   Blockchain chain = makeChain();
   const Transaction pay({A_COINS}, {{B, 70}, {A, 30}}, 1);
   const Transaction other({B_COINS}, {{C, 50}}, 2);
   const Block valid = minedBlock(chain, {pay});
   std::string reason;

   // Content swapped after mining
   CHECK(!chain.addBlock(Block(valid.getHeader(), {other}), reason));
   CHECK(reason == "txRoot mismatch");
   checkUnchanged(chain, 1, 100, 50);

   // prev_hash changed
   Block::Header header = valid.getHeader();
   header.prevHash = std::string(64, '1');
   CHECK(!chain.addBlock(Block(header, {pay}), reason));
   CHECK(reason == "prevHash does not match chain head");
   checkUnchanged(chain, 1, 100, 50);

   // Nonce that does not meet the difficulty
   Block badPow = valid;
   std::uint64_t nonce = valid.getHeader().nonce;
   while(badPow.hasValidPow()) badPow.setNonce(++nonce);
   CHECK(!chain.addBlock(badPow, reason));
   CHECK(reason == "hash does not meet difficulty");
   checkUnchanged(chain, 1, 100, 50);

   // Miner lowers the difficulty on its own
   Block easy(chain.getHeadHash(), std::time(nullptr), chain.getVersion(), {pay}, 1);
   mine(easy);
   CHECK(!chain.addBlock(easy, reason));
   CHECK(reason == "wrong difficulty");
   checkUnchanged(chain, 1, 100, 50);

   // Mined block that pays someone who is not a user
   const Transaction toNobody({A_COINS}, {{NOBODY, 70}, {A, 30}}, 3);
   CHECK(!chain.addBlock(minedBlock(chain, {toNobody}), reason));
   CHECK(contains(reason, "unknown receiver"));
   checkUnchanged(chain, 1, 100, 50);

   // The untouched block is still accepted
   CHECK(chain.addBlock(valid, reason));
}

static bool hasTransaction(const Block& block, const Transaction& tx) {
   for(const Transaction& inBlock : block.getTransactions()) {
      if(inBlock.getId() == tx.getId()) return true;
   }
   return false;
}

using MineAll = RaceResult (*)(std::vector<Candidate>&, std::uint64_t, std::uint64_t);

// Candidates don't share state, and only the winner changes the chain. Runs with either way of mining
static void checkCandidates(MineAll mineAll) {
   Blockchain chain = makeChain();
   const Transaction first({A_COINS}, {{B, 70}, {A, 30}}, 1);
   const Transaction second({A_COINS}, {{C, 60}, {A, 40}}, 2);   // conflicts with `first`
   const Transaction fromB({B_COINS}, {{C, 50}}, 3);
   std::vector<Transaction> pool{first, second, fromB};
   Random rng(1);

   std::vector<Candidate> candidates = buildCandidates(pool, chain, rng, CANDIDATES_PER_BLOCK);
   CHECK(candidates.size() == CANDIDATES_PER_BLOCK);
   checkUnchanged(chain, 1, 100, 50);

   // Each candidate holds fromB and exactly one of the conflicting pair, and is valid on its own
   for(const Candidate& candidate : candidates) {
      CHECK(candidate.block.getHeader().prevHash == chain.getHeadHash());
      CHECK(candidate.block.getTransactions().size() == 2);
      CHECK(hasTransaction(candidate.block, fromB));
      CHECK(hasTransaction(candidate.block, first) != hasTransaction(candidate.block, second));

      Blockchain copy = chain;
      Block block = candidate.block;
      mine(block);
      std::string reason;
      CHECK(copy.addBlock(block, reason));
   }

   // Own limit, so a low MAX_ATTEMPTS_PER_BLOCK can't make the test fail by chance
   const RaceResult race = mineAll(candidates, FIRST_ROUND_BUDGET, 100000000);
   CHECK(race.winner >= 0);
   if(race.winner < 0) return;

   const Candidate& winner = candidates[race.winner];
   CHECK(winner.solved && winner.block.hasValidPow());

   std::string reason;
   CHECK(chain.addBlock(winner.block, reason));

   // Only the winner's transactions are confirmed, losers leave nothing behind
   for(const Transaction& tx : {first, second, fromB}) {
      CHECK(chain.isIncluded(tx.getId()) == hasTransaction(winner.block, tx));
   }
   CHECK(chain.getConfirmed().total() == 150);
}

void candidates() { checkCandidates(mineCandidates); }
void parallelCandidates() {
   // Parallel mining has no budget, so the wrapper ignores it
   checkCandidates([](std::vector<Candidate>& candidates, std::uint64_t, std::uint64_t maxTotalAttempts) {
      return mineCandidatesParallel(candidates, maxTotalAttempts);
   });
}

// Difficulty 64 needs a hash of only zeros, which never happens, so these candidates use up every budget
static std::vector<Candidate> unsolvable(std::size_t count) {
   std::vector<Candidate> candidates;
   for(std::size_t i = 0; i < count; ++i) {
      candidates.push_back({Block(std::string(64, '0'), static_cast<std::int64_t>(i), 2, {}, 64)});
   }
   return candidates;
}

// CANDIDATES_PER_BLOCK candidates for block 1, each paying B a different amount
static std::vector<Candidate> solvable(const Blockchain& chain) {
   std::vector<Candidate> candidates;
   for(unsigned int i = 0; i < CANDIDATES_PER_BLOCK; ++i) {
      candidates.push_back({chain.createBlock({Transaction({A_COINS}, {{B, 1 + i}, {A, 99 - i}}, i + 1)})});
   }
   return candidates;
}

// True if no nonce in [from, to) meets the block's difficulty
static bool noValidNonce(Block block, std::uint64_t from, std::uint64_t to) {
   for(std::uint64_t nonce = from; nonce < to; ++nonce) {
      block.setNonce(nonce);
      if(block.hasValidPow()) return false;
   }
   return true;
}

// mine() counts the nonce up from 0 and stops at the first one that meets the difficulty
void mineFindsFirstNonce() {
   const Blockchain chain = makeChain();
   Block block = chain.createBlock({Transaction({A_COINS}, {{B, 70}, {A, 30}}, 1)});
   const MiningResult result = mine(block);
   const std::uint64_t nonce = block.getHeader().nonce;

   CHECK(block.hasValidPow());
   CHECK(result.attempts == nonce + 1);
   CHECK(noValidNonce(block, 0, nonce));
}

// Budgets double each round, every round continues at the first untried nonce, and the overall limit is exact
void sequentialBudgetsAndLimit() {
   // Round 1: 10 each (30 in total), round 2: 20 each (90), round 3: #1 gets the last 10 and the limit is reached
   std::vector<Candidate> stuck = unsolvable(3);
   const RaceResult limited = mineCandidates(stuck, 10, 100);
   CHECK(limited.winner == -1);
   CHECK(limited.rounds == 3);
   CHECK(limited.totalAttempts == 100);
   CHECK(stuck[0].attempts == 40 && stuck[1].attempts == 30 && stuck[2].attempts == 30);
   for(const Candidate& candidate : stuck) {
      CHECK(!candidate.solved);
      CHECK(candidate.nextNonce == candidate.attempts);   // nonces 0, 1, 2, ..., none skipped or tried twice
   }

   // The first candidate in order to reach a valid nonce wins, at its first valid nonce. Nobody tried a valid
   // nonce before that, otherwise they would have won
   Blockchain chain = makeChain();
   std::vector<Candidate> race = solvable(chain);
   const RaceResult result = mineCandidates(race, FIRST_ROUND_BUDGET, 100000000);
   CHECK(result.winner >= 0);
   if(result.winner < 0) return;

   // Budgets of all finished rounds: 1000, 1000 + 2000, 1000 + 2000 + 4000, ...
   const std::uint64_t allRounds = FIRST_ROUND_BUDGET * ((1ull << result.rounds) - 1);
   const std::uint64_t earlierRounds = FIRST_ROUND_BUDGET * ((1ull << (result.rounds - 1)) - 1);
   std::uint64_t total = 0;
   for(int i = 0; i < race.size(); ++i) {
      const Candidate& candidate = race[i];
      total += candidate.attempts;
      CHECK(candidate.solved == (i == result.winner));
      CHECK(candidate.nextNonce == candidate.attempts);
      if(i < result.winner) CHECK(candidate.attempts == allRounds);
      if(i > result.winner) CHECK(candidate.attempts == earlierRounds);

      const std::uint64_t triedWithoutSuccess = i == result.winner ? candidate.attempts - 1 : candidate.attempts;
      CHECK(noValidNonce(candidate.block, 0, triedWithoutSuccess));
   }
   CHECK(total == result.totalAttempts);
   CHECK(race[result.winner].block.getHeader().nonce == race[result.winner].attempts - 1);

   // Nothing to mine: no winner, no rounds, no attempts
   std::vector<Candidate> none;
   const RaceResult empty = mineCandidates(none, FIRST_ROUND_BUDGET, 100);
   CHECK(empty.winner == -1 && empty.rounds == 0 && empty.totalAttempts == 0);
}

// Every thread mines an equal share of the limit and no more; once one thread wins, the others stop
void parallelLimitAndStop() {
   std::vector<Candidate> stuck = unsolvable(CANDIDATES_PER_BLOCK);
   const RaceResult limited = mineCandidatesParallel(stuck, 1003);   // 200 each, the last 3 stay unused
   CHECK(limited.winner == -1);
   CHECK(limited.totalAttempts == 1000);
   for(const Candidate& candidate : stuck) {
      CHECK(!candidate.solved && candidate.attempts == 200 && candidate.nextNonce == 200);
   }

   // Without the stop every thread would go on to its share of 20,000,000. Difficulty 3 needs about 4,096 attempts
   // on average, so a race that used a million has not stopped
   Blockchain chain = makeChain();
   std::vector<Candidate> race = solvable(chain);
   const RaceResult result = mineCandidatesParallel(race, 100000000);
   CHECK(result.winner >= 0);
   CHECK(result.totalAttempts < 1000000);
   if(result.winner >= 0) CHECK(race[result.winner].solved && race[result.winner].block.hasValidPow());

   std::uint64_t total = 0;
   for(const Candidate& candidate : race) total += candidate.attempts;
   CHECK(total == result.totalAttempts);

   std::vector<Candidate> none;
   const RaceResult empty = mineCandidatesParallel(none, 100);
   CHECK(empty.winner == -1 && empty.totalAttempts == 0);
}

template <typename F>
static bool throwsInvalidArgument(F function) {
   try { function(); }
   catch(const std::invalid_argument&) { return true; }
   return false;
}

// Serialized forms match the README, hashing them is deterministic and every header field counts
void formatsAndDeterminism() {
   // Transaction: <transactionId>:<index>;...|<ownerPK>:<amount>;...|<sequence>
   const Transaction pay({A_COINS, B_COINS}, {{C, 120}, {A, 30}}, 7);
   CHECK(pay.serialize() == FUNDING.getId() + ":0;" + FUNDING.getId() + ":1|" + C + ":120;" + A + ":30|7");
   CHECK(FUNDING.serialize() == "|" + A + ":100;" + B + ":50|0");
   CHECK(pay.getId() == hash::hashFunction(pay.serialize()));
   CHECK(pay.getId().size() == 64);
   CHECK(pay.getId() == Transaction({A_COINS, B_COINS}, {{C, 120}, {A, 30}}, 7).getId());   // same fields, same ID
   CHECK(pay.getId() != Transaction({A_COINS, B_COINS}, {{C, 120}, {A, 30}}, 8).getId());   // the sequence tells them apart

   // Block header: <prevHash>|<timestamp>|<version>|<txRoot>|<difficulty>|<nonce>
   const Block::Header header{std::string(64, '0'), 1762500000, 2, hash::hashFunction(""), 3, 42};
   CHECK(header.serialize() == std::string(64, '0') + "|1762500000|2|" + hash::hashFunction("") + "|3|42");
   CHECK(header.serializePrefix() + "42" == header.serialize());   // what the miner writes the nonce after
   const Block block(header, {});
   CHECK(block.hash() == hash::hashFunction(header.serialize()));
   CHECK(block.hash() == Block(header, {}).hash());   // a fixed header gives the same hash every time
   CHECK(block.hash().size() == 64);

   // Changing any one field changes the hash
   std::vector<Block::Header> changed(6, header);
   changed[0].prevHash = std::string(64, '1');
   changed[1].timestamp += 1;
   changed[2].version += 1;
   changed[3].txRoot = hash::hashFunction("other");
   changed[4].difficulty += 1;
   changed[5].nonce += 1;
   for(const Block::Header& other : changed) CHECK(Block(other, {}).hash() != block.hash());

   // The separators keep neighbouring numbers apart: timestamp 1 and version 23 is not timestamp 12 and version 3
   Block::Header first = header;
   Block::Header second = header;
   first.timestamp = 1;
   first.version = 23;
   second.timestamp = 12;
   second.version = 3;
   CHECK(first.serialize() != second.serialize());

   // Hex decoding used by the Merkle tree
   CHECK(Block::hexToBytes("00ff10") == std::string("\x00\xff\x10", 3));
   CHECK(throwsInvalidArgument([] { Block::hexToBytes("abc"); }));
   CHECK(throwsInvalidArgument([] { Block::hexToBytes("zz"); }));
}

int main() {
   const std::vector<std::pair<const char*, void (*)()>> tests = {
      {"merkle tree with 0, 1, 2, 3 and 5 transactions", merkleTree},
      {"valid payment in a small block", validPayment},
      {"pool rejects bad transactions", poolRejectsBadTransactions},
      {"overspending from one coin", overspending},
      {"duplicates and replays", duplicatesAndReplays},
      {"tampered blocks", tamperedBlocks},
      {"candidates are separate, only the winner counts", candidates},
      {"parallel: candidates are separate, only the winner counts", parallelCandidates},
      {"mining stops at the first valid nonce", mineFindsFirstNonce},
      {"sequential race: budgets double, nonces continue, limit holds", sequentialBudgetsAndLimit},
      {"parallel race: equal shares of the limit, losers stop", parallelLimitAndStop},
      {"serialization formats and hash determinism", formatsAndDeterminism},
   };

   for(const auto& [name, test] : tests) {
      const int before = failures;
      test();
      std::cout << (failures == before ? "[ok]   " : "[FAIL] ") << name << "\n";
   }

   std::cout << "\n" << checks - failures << "/" << checks << " checks passed\n";
   return failures == 0 ? 0 : 1;
}
