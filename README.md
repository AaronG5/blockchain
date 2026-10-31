# Simplified blockchain simulation

## Requirements

C++11 or higher is required in order for the simulation to work.

## Transactions

Transactions follow the UTXO model. A user's balance is calculated from unspent transaction records (UTXO). Users send funds to each other by referencing previously completed and unspent transaction records. A transaction record can only be used once. When it gets used for a new transaction, the entire amount is spent - part of it goes to the recipient user, other part goes back to user, who created the transaction.

Transactions are serialized in this order:

```
<transactionId>:<index>;<transactionId>:<index>|<ownerPK>:<amount>;<ownerPK>:<amount>|<sequence>
```

### Block header serialization

Block headers are serialized in this order:

```
<prevHash>|<timestamp>|<version>|<txRoot>|<difficulty>|<nonce>
```
