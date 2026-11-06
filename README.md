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
difficulty 3). Apple M3, Apple clang 21, `-O2`. Mean ± standard deviation from [`benchmark.csv`](/benchmark.csv):

| Mode       | Mining time     | Total time      | Attempts         | Blocks      |
| ---------- | --------------- | --------------- | ---------------- | ----------- |
| sequential | 0.374 ± 0.037 s | 1.161 ± 0.054 s | 387,043 ± 29,727 | 105.2 ± 1.3 |
| parallel   | 0.139 ± 0.033 s | 0.937 ± 0.053 s | 440,735 ± 59,278 | 104.2 ± 0.4 |

Parallel mining is about 2.7× faster. The total time improves less, because generating users and transactions
(~0.8 s) runs on one thread. Block contents differ slightly between runs (timestamps change which candidate
wins), so the number of blocks and attempts varies.

## Third-party code

[`stb_image.h`](https://github.com/nothings/stb) by Sean Barrett (MIT or public domain), used to load `image.png`.
