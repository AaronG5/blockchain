# Simplified blockchain simulation

## Requirements

C++11 or higher is required in order for the simulation to work.

### Transaction serialization

Transactions are serialized in this order:

```
<sender>|<receiver>|<amount>|<sequence>
```

### Block header serialization

Block headers are serialized in this order:

```
<prevHash>|<timestamp>|<version>|<txRoot>|<difficulty>|<nonce>
```
