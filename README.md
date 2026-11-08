# Simplified blockchain simulation

A single-process blockchain simulation: it generates users and transactions, builds 5 candidate blocks per height with Merkle roots, mines them with Proof-of-Work (sequentially or in parallel), validates the winner and updates the state. Funds use the UTXO model. Current version: **v0.2**.

## Running

**C++17** required, no dependencies to install. Run from the repository root, because the hash function loads `image.png`.

```sh
make run                     # default settings
make run ARGS="7 5 8 1"      # small example
make test                    # run tests
```

Arguments: `<seed> <users> <transactions> <bad_per_kind> --parallel --benchmark`, defaults `123 1000 10000 5`.

- `<seed>` - used for RNG initialization.
- `<users>` - amount of users generated.
- `<transactions>` - amount of transactions generated.
- `<bad_per_kind>` - amount of each type of invalid transaction generated (double spend, tampered ID, zero amount, unknown receiver). Calculated as `<bad_per_kind> * 4 = total_invalid_tx`.
- `--parallel` argument runs the simulation using parallel mining.
- `--benchmark` argument runs the simulation in a benchmarking mode. Using the same configured run arguments, sequencial and parallel mining are run 5 times each, their simulation benchmark results are outputed to [`benchmark.csv`](/benchmark.csv)

The program prints every block and a final summary to the console, writes a copy of the output to
[`output.txt`](/output.txt). The initial and final balances are written to `initial_balances.csv` and `final_balances.csv`.

## Hash function

My own hash from the previous assignment: [`hash.hpp`](/src/headers/hash.hpp). No big changes were done to the original hash code.

## Model

- **Users:** name, unique public key (a simulated identifier) and an initial balance from 100 to 1,000,000.
- **UTXO:** a transaction spends whole earlier outputs (inputs) and creates new outputs, with change back to the sender. Inputs and outputs must have equal totals. The sender is the owner of the inputs.
- **Initial funds:** one funding transaction with no inputs and one output per user. It is not in a block.
- **Transaction ID:** hash of the serialized transaction:

   ```text
   <transactionId>:<index>;...|<ownerPK>:<amount>;...|<sequence>
   ```

- **Block header:** the block hash is the hash of:

   ```text
   <prevHash>|<timestamp>|<version>|<txRoot>|<difficulty>|<nonce>
   ```

   `timestamp` is UTC Unix seconds, `version` is 2. A block holds at most 100 transactions.
   `txRoot` is the root node of a Merkle tree. The leaves are the transaction IDs in block order, decoded from hex to bytes. A parent is H(left bytes || right bytes), and a lone last node is paired with itself. One transaction gives that transaction's ID, and an empty block (genesis) gives H("")

- **Proof-of-Work:** the block hash must start with `difficulty` (3) hex zeros. The nonce counts up from 0.
- **Genesis:** no transactions, `prevHash` of 64 zeros, `txRoot` = `H("")`. Mined separately, without candidates.

## Mining

5 candidates are built for each block height, each from the same `prev_hash` and confirmed state, each with its own temporary state copy, and up to 100 differing transactions each.

- An overall attempt limit per block (500000), after which the run stops;
- The winner is checked again by addBlock, and only its transactions are included;
- The losing candidates are dropped.

### Sequential mode

- Candidates take turns in order (#1 to #5), each with a budget of 1000 attempts;
- If nobody finds a nonce, the budget doubles and the next round continues from the untried nonces;
- The first candidate to succeed in order wins.

### Parallel mode

- One thread per candidate, no rounds;
- The first thread to find a nonce wins and stops the others.

### Why not P2P?

The whole blockchain is a simulation, it runs in one process, with no network, no separate nodes, no forks or reorganizations, and the winner is chosen locally by a fixed rule.

## Validation

1. **Entering the pool:** a transaction with an invalid ID, missing inputs or outputs, a zero amount, an unknown receiver or a repeated ID is rejected with a reason.
2. **Selection:** the pool is shuffled and transactions are applied one by one to a copy of the confirmed state, up to 100. Those that fail, like unknown or spent outputs, are deferred and stay in the pool, totals are checked for overflows.
3. **Accepting a block:** the mined block is checked again: header fields, PoW, `txRoot`, IDs, unknown receivers, and every transaction applied in order. If anything fails, nothing changes.

The run stops when no transaction in the pool can be applied. Each rejected and remaining transaction is listed
with its reason. To test the rules, 5 invalid transactions of each kind are added: double spend, tampered ID,
zero amount and unknown receiver.

## Reproducibility

The same seed gives the same users, balances, transactions and initial state. Block hashes and nonces change between runs because of the timestamp.

## Results

### Example run

Default settings, sequential mining (`make run`, seed 123), from [`output.txt`](/output.txt). Block 3 needed a
second round: in round 1 all five candidates used their 1000 attempts without a nonce, in round 2 #1 and #2 used
2000 more each, then #3 found one at nonce 1362.

```text
Block 3
   transactions: 100
   prev_hash:    00089499dcc1433ecf88b4a67fad7a2cfb54f26320af556b9c8e8adff7a2217f
   tx_root:      3c9b7147e76a4863c6025e8e5d328953120138b07a38d66fd73f43e7084cec00
   hash:         000a36cd0983e582b3cc5e56ff5101572b5b4632bd881f968e25c0305f452988
   difficulty:   3
   nonce:        1362
   attempts:     1363
   mining time:  0.002157 s
   rounds:       2
   candidates:
      #1: 100 txs,   3000 attempts, 0.007027 s
      #2: 100 txs,   3000 attempts, 0.005195 s
      #3: 100 txs,   1363 attempts, 0.002157 s  <- winner
      #4: 100 txs,   1000 attempts, 0.001473 s
      #5: 100 txs,   1000 attempts, 0.001334 s
   left in pool: 9705

Summary
   seed:                  123
   users:                 1000
   transactions:          10020
   blocks (with genesis): 105
   included:              10000
   rejected:              15
   remaining in pool:     5
   initial funds:         499911044
   final funds:           499911044
   funds match:           YES
   counts add up:         YES
   candidates per block:  5
   first round budget:    1000 attempts per candidate, doubled each round
   mining mode:           sequential
   attempts (candidates): 405321
   candidate mining time: 0.695 s
   total time:            1.707 s
   initial balances:      initial_balances.csv
   final balances:        final_balances.csv
```

76 blocks were found in the first round, 27 needed a second and 1 a third.

20 invalid transactions are added on purpose, 5 of each kind. 15 are rejected when entering the pool
(5 × invalid id, zero amount output and unknown receiver). The 5 double spends are valid on their own, so they
enter the pool, but each conflicts with the transaction it copies: whichever confirms first spends the inputs, and
the other stays in the pool as `unknown or spent output`. That gives 10020 = 10000 included + 15 rejected +
5 remaining. Both lists are printed with their reasons at the end of the output.

### Tests

`make test`: 8 tests, 151/151 checks pass.

| Test                              | Scenario (task section 5)                                                          | Result |
| --------------------------------- | ---------------------------------------------------------------------------------- | ------ |
| merkle tree                       | Merkle roots for 1, 2, 3, 5 transactions; order or content change changes the root | ok     |
| valid payment                     | valid payment, last block under 100                                                | ok     |
| pool rejects bad transactions     | unknown receiver, zero amount, changed ID                                          | ok     |
| overspending                      | two payments from one sender exceed the balance                                    | ok     |
| duplicates and replays            | duplicated ID, replay of a confirmed payment                                       | ok     |
| tampered blocks                   | changed content, `prev_hash`, invalid PoW                                          | ok     |
| candidates (sequential, parallel) | losing candidates, candidate states don't mix                                      | ok     |

Negative amounts can't be represented, since `amount` is unsigned.

### Sequential vs parallel mining

`make benchmark`: 5 runs per mode, same config (default settings: seed 123, 1000 users, 10000 transactions,
difficulty 3). Apple M3, Apple clang 21, `-O2`. Mean ± standard deviation from [`benchmark-v0.2.csv`](/benchmark-v0.2.csv):

| Mode       | Mining time     | Total time      | Attempts         | Blocks      |
| ---------- | --------------- | --------------- | ---------------- | ----------- |
| sequential | 0.477 ± 0.021 s | 1.325 ± 0.046 s | 412,538 ± 23,597 | 105.2 ± 1.6 |
| parallel   | 0.150 ± 0.014 s | 0.988 ± 0.019 s | 425,802 ± 33,868 | 104.2 ± 0.8 |

Parallel mining is about 3.2× faster. The total time improves less, because generating users and transactions
(~0.8 s) runs on one thread. Block contents differ slightly between runs (timestamps change which candidate
wins), so the number of blocks and attempts varies.

### v0.2 and v0.3 comparison

#### <u>Mining speed</u>

`make benchmark` with default settings, 5 runs per mode, mean ± standard deviation. Apple M3, Apple clang 21, `-O2`.
v0.2 was measured from the `blockchain-v0.2` tag with only the new benchmark (B) added, so both versions report
attempts/s and fixed work. Results: [`benchmark-v0.2.csv`](/benchmark-v0.2.csv),
[`benchmark-v0.3.csv`](/benchmark-v0.3.csv).

| Metric                 | v0.2            | v0.3            | Change |
| ---------------------- | --------------- | --------------- | ------ |
| Sequential mining time | 0.477 ± 0.021 s | 0.409 ± 0.024 s | −14%   |
| Sequential total time  | 1.325 ± 0.046 s | 1.196 ± 0.056 s | −10%   |
| Sequential attempts/s  | 0.86 ± 0.01 M   | 0.97 ± 0.03 M   | +12%   |
| Parallel mining time   | 0.150 ± 0.014 s | 0.126 ± 0.005 s | −16%   |
| Parallel total time    | 0.988 ± 0.019 s | 0.923 ± 0.070 s | −7%    |
| Parallel attempts/s    | 2.84 ± 0.25 M   | 3.28 ± 0.25 M   | +15%   |
| Fixed work, sequential | 1.07 ± 0.03 M/s | 1.27 ± 0.04 M/s | +18%   |
| Fixed work, parallel   | 3.81 ± 0.17 M/s | 4.12 ± 0.75 M/s | +8%    |

- **Attempts per second** went up because an attempt no longer rebuilds the header string or allocates memory, only
  the nonce digits are rewritten, and parallel threads no longer write next to each other on every attempt (M2).
- **Mining time** follows attempts/s, but also depends on how many attempts the timestamps happen to need. The
  fixed-work runs mine the same 500,000 attempts every time, so they show the speed alone.
- **Total time** dropped more than mining time, because building candidates got cheaper (M1): selection checks
  don't allocate and Merkle nodes reuse one buffer. The selected transactions are exactly the same as in v0.2.
- **Blocks and attempts per run** didn't change (about 105 blocks, 390–450 thousand attempts), since the difficulty
  and the rules are the same.
- A second pair of runs gave the same picture: fixed work +19% sequential and +20% parallel, total time
  −8% sequential and −11% parallel.

#### <u>Test coverage</u>

`make test` and `make coverage` (clang source-based coverage, without `stb_image.h` and the tests themselves).

| Metric    | v0.2            | v0.3            |
| --------- | --------------- | --------------- |
| Tests     | 8               | 22              |
| Checks    | 151/151 passed  | 333/333 passed  |
| Lines     | 69.0% (408/591) | 96.8% (599/619) |
| Branches  | 81.3% (174/214) | 92.6% (252/272) |
| Functions | 76.3% (58/76)   | 98.8% (83/84)   |

The 8 v0.2 tests are unchanged and still pass. The new ones cover the generator (`generator.hpp` from 7% to 98% of
lines), the whole mining cycle in both modes, every rejection reason for transactions and blocks, the race limits and
budgets, and the exact serialization formats. The totals changed too, because v0.3 has new code and llvm-cov only
counts some of a header's lines and branches once its functions run. Still not covered: `saveBalances`, the two
overflow checks (they can't trigger with 32-bit amounts) and parts of `hash.hpp`.

## Third-party code

[`stb_image.h`](https://github.com/nothings/stb) by Sean Barrett (MIT or public domain), used to load `image.png`.
