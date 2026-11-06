#include <cstdint>
#include <ctime>
#include <iostream>
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
   };

   for(const auto& [name, test] : tests) {
      const int before = failures;
      test();
      std::cout << (failures == before ? "[ok]   " : "[FAIL] ") << name << "\n";
   }

   std::cout << "\n" << checks - failures << "/" << checks << " checks passed\n";
   return failures == 0 ? 0 : 1;
}
