# Simplified blockchain simulation

A single-process blockchain simulation: it generates users and transactions, mines blocks with Proof-of-Work, validates them and updates the state. Funds use the UTXO model. Current version: **v0.1**.

## Running

**C++17** required, no dependencies to install. Run from the repository root, because the hash function loads `image.png`.

```sh
make run                     # default settings
make run ARGS="7 5 8 1"      # small example
```

Arguments: `<seed> <users> <transactions> <bad per kind>`, defaults `123 1000 10000 5`.

The program prints every block and a final summary, and writes `initial_balances.csv` and `final_balances.csv`.

## Hash function

My own hash from the previous assignment: tag `v0.2.0`, file `Project_1/hash_v2.hpp`, used here as
`src/headers/hash.hpp`. No big changes were done to the original hash code.

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

   `timestamp` is UTC Unix seconds, `version` is 1, and `txRoot` is the hash of the block's transaction IDs joined
   with `|`. A block holds at most 100 transactions.

- **Proof-of-Work:** the block hash must start with `difficulty` (3) hex zeros. The nonce counts up from 0.
- **Genesis:** no transactions, `prevHash` of 64 zeros, `txRoot` = `H("")`. It is mined like any other block.

## Validation

1. **Entering the pool:** a transaction with an invalid ID, missing inputs or outputs, a zero amount, an unknown receiver or a repeated ID is rejected with a reason.
2. **Selection:** the pool is shuffled and transactions are applied one by one to a copy of the confirmed state, up to 100. Those that fail, like unknown or spent outputs, are deferred and stay in the pool.
3. **Accepting a block:** the mined block is checked again: header fields, PoW, `txRoot`, IDs, and every transaction applied in order. If anything fails, nothing changes.

The run stops when no transaction in the pool can be applied. Each rejected and remaining transaction is listed
with its reason. To test the rules, 5 invalid transactions of each kind are added: double spend, tampered ID,
zero amount and unknown receiver.

## Reproducibility

The same seed gives the same users, balances, transactions and block contents. Block hashes and nonces change between runs because of the timestamp.

## Results

Seed 42 (`make run ARGS=42`):

```text
Block 1
   transactions: 100
   prev_hash:    00046bb219fa68d2bb5e8960238bcbf955f1db62a340825a38d0114810e3d63b
   tx_root:      933384458a4b7f7304950aade9da80af6e8e6eb4919cf90e467d671d830a6c24
   hash:         0001b4fa7940a1f32c4cb078f28ad8abe0643b4ad0b5d80877a3bc2d87937763
   difficulty:   3
   nonce:        5304
   attempts:     5305
   mining time:  0.010835 s

Summary
   seed:                  42
   users:                 1000
   transactions:          10020
   blocks (with genesis): 106
   included:              10000
   rejected:              15
   remaining in pool:     5
   initial funds:         490596517
   final funds:           490596517
   funds match:           yes
   counts add up:         yes
   total time:            1.333 s
```

The 5 remaining transactions are the losing sides of the double spends.

## Third-party code

[`stb_image.h`](https://github.com/nothings/stb) by Sean Barrett (MIT or public domain), used to load `image.png`.
